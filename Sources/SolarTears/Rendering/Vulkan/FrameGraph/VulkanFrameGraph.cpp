#include "VulkanFrameGraph.hpp"
#include "../VulkanWorkerCommandBuffers.hpp"
#include "../VulkanFunctions.hpp"
#include "../VulkanSwapChain.hpp"
#include "../VulkanDeviceQueues.hpp"
#include "../Scene/VulkanScene.hpp"
#include "../../../Core/ThreadPool.hpp"
#include "../../Common/RenderingUtils.hpp"
#include <array>
#include <latch>

Vulkan::FrameGraph::FrameGraph(VkDevice device, const FrameGraphConfig& frameGraphConfig): ModernFrameGraph(frameGraphConfig), mDeviceRef(device)
{
	mImageMemory = VK_NULL_HANDLE;

	CreateSemaphores();
}

Vulkan::FrameGraph::~FrameGraph()
{
	for(size_t i = 0; i < mImageViews.size(); i++)
	{
		SafeDestroyObject(vkDestroyImageView, mDeviceRef, mImageViews[i]);
	}

	for(size_t i = 0; i < mBackbufferImageSpan.Begin; i++)
	{
		SafeDestroyObject(vkDestroyImage, mDeviceRef, mImages[i]);
	}

	for(size_t i = mBackbufferImageSpan.End; i < mImages.size(); i++)
	{
		SafeDestroyObject(vkDestroyImage, mDeviceRef, mImages[i]);
	}

	for(size_t i = 0; i < Utils::InFlightFrameCount; i++)
	{
		SafeDestroyObject(vkDestroySemaphore, mDeviceRef, mGraphicsToPresentSemaphores[i]);
	}

	SafeDestroyObject(vkFreeMemory, mDeviceRef, mImageMemory);
}

void Vulkan::FrameGraph::Traverse(ThreadPool* threadPool, WorkerCommandBuffers* commandBuffers, RenderableScene* scene, DeviceQueues* deviceQueues, VkFence traverseFence, uint32_t frameIndex, uint32_t swapchainImageIndex, VkSemaphore preTraverseSemaphore, VkSemaphore* outPostTraverseSemaphore)
{
	SwitchSwapchainImages(currentSwapchainImageIndex);

	VkSemaphore lastTraverseSemaphore = nullptr;
	if(mGraphicsPassSpans.size() > 0)
	{
		VkSemaphore graphicsSemaphore = mGraphicsToPresentSemaphores[currentFrameResourceIndex];
		VkFence     graphicsFence     = traverseFence; //Signal the fence after graphics command submission if there are no compute passes afterwards (which is always true for now)

		std::latch graphicsPassLatch((uint32_t)(mGraphicsPassSpans.size() - 1));
		for(size_t dependencyLevelSpanIndex = 0; dependencyLevelSpanIndex < mGraphicsPassSpans.size() - 1; dependencyLevelSpanIndex++)
		{
			struct JobData
			{
				const WorkerCommandBuffers* CommandBuffers;
				const FrameGraph*           FrameGraph;
				const RenderableScene*      Scene;
				std::latch*                 Waitable;
				uint32_t                    DependencyLevelSpanIndex;
				uint32_t                    FrameResourceIndex;
			}
			jobData =
			{
				.CommandBuffers           = commandBuffers,
				.FrameGraph               = this,
				.Scene                    = scene,
				.Waitable                 = &graphicsPassLatch,
				.DependencyLevelSpanIndex = (uint32_t)dependencyLevelSpanIndex,
				.FrameResourceIndex       = currentFrameResourceIndex
			};

			auto executePassJob = [](void* userData, uint32_t userDataSize)
			{
				UNREFERENCED_PARAMETER(userDataSize);

				JobData* threadJobData = reinterpret_cast<JobData*>(userData);
				const FrameGraph* that = threadJobData->FrameGraph;

				VkCommandBuffer graphicsCommandBuffer = threadJobData->CommandBuffers->GetThreadGraphicsCommandBuffer(threadJobData->DependencyLevelSpanIndex, threadJobData->FrameResourceIndex);
				VkCommandPool   graphicsCommandPool   = threadJobData->CommandBuffers->GetThreadGraphicsCommandPool(threadJobData->DependencyLevelSpanIndex, threadJobData->FrameResourceIndex);

				that->BeginCommandBuffer(graphicsCommandBuffer, graphicsCommandPool);
				that->RecordGraphicsPasses(graphicsCommandBuffer, threadJobData->Scene, threadJobData->DependencyLevelSpanIndex);
				that->EndCommandBuffer(graphicsCommandBuffer);

				threadJobData->Waitable->count_down();
			};

			threadPool->EnqueueWork(executePassJob, &jobData, sizeof(JobData));
		}

		VkCommandBuffer mainGraphicsCommandBuffer = commandBuffers->GetMainThreadGraphicsCommandBuffer(currentFrameResourceIndex);
		VkCommandPool   mainGraphicsCommandPool   = commandBuffers->GetMainThreadGraphicsCommandPool(currentFrameResourceIndex);
		
		BeginCommandBuffer(mainGraphicsCommandBuffer, mainGraphicsCommandPool);
		RecordGraphicsPasses(mainGraphicsCommandBuffer, scene, (uint32_t)(mGraphicsPassSpans.size() - 1));
		EndCommandBuffer(mainGraphicsCommandBuffer);

		graphicsPassLatch.wait();

		for(size_t i = 0; i < mGraphicsPassSpans.size() - 1; i++)
		{
			mFrameRecordedGraphicsCommandBuffers[i] = commandBuffers->GetThreadGraphicsCommandBuffer((uint32_t)i, currentFrameResourceIndex); //The command buffer that was used to record the command
		}

		mFrameRecordedGraphicsCommandBuffers[mGraphicsPassSpans.size() - 1] = mainGraphicsCommandBuffer;
		deviceQueues->GraphicsQueueSubmit(mFrameRecordedGraphicsCommandBuffers.data(), mGraphicsPassSpans.size(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, preTraverseSemaphore, graphicsSemaphore, graphicsFence);

		lastTraverseSemaphore = graphicsSemaphore;
	}

	if(outPostTraverseSemaphore)
	{
		*outPostTraverseSemaphore = lastTraverseSemaphore;
	}
}

void Vulkan::FrameGraph::CreateSemaphores()
{
	for(uint32_t i = 0; i < Utils::InFlightFrameCount; i++)
	{
		VkSemaphoreCreateInfo semaphoreCreateInfo;
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphoreCreateInfo.pNext = nullptr;
		semaphoreCreateInfo.flags = 0;

		ThrowIfFailed(vkCreateSemaphore(mDeviceRef, &semaphoreCreateInfo, nullptr, &mGraphicsToPresentSemaphores[i]));
	}
}

void Vulkan::FrameGraph::SwitchSwapchainImages(uint32_t swapchainImageIndex)
{
	//Update barriers
	for(size_t i = 0; i < mSwapchainBarrierIndices.size(); i++)
	{
		mImageBarriers[i].image = mImages[mBackbufferImageSpan.Begin + swapchainImageIndex];
	}
}

void Vulkan::FrameGraph::BeginCommandBuffer(VkCommandBuffer cmdBuffer, VkCommandPool cmdPool) const
{
	ThrowIfFailed(vkResetCommandPool(mDeviceRef, cmdPool, 0));

	VkCommandBufferBeginInfo cmdBufferBeginInfo;
	cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufferBeginInfo.pNext = nullptr;
	cmdBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	cmdBufferBeginInfo.pInheritanceInfo = nullptr;

	ThrowIfFailed(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo));
}

void Vulkan::FrameGraph::EndCommandBuffer(VkCommandBuffer cmdBuffer) const
{
	ThrowIfFailed(vkEndCommandBuffer(cmdBuffer));
}

void Vulkan::FrameGraph::RecordGraphicsPasses(VkCommandBuffer graphicsCommandBuffer, const RenderableScene* scene, uint32_t dependencyLevelSpanIndex, uint32_t frameIndex, uint32_t swapchainImageIndex) const
{
	Span<uint32_t> levelSpan = mGraphicsPassSpans[dependencyLevelSpanIndex];
	for(uint32_t passSpanIndex = levelSpan.Begin; passSpanIndex < levelSpan.End; passSpanIndex++)
	{
		BarrierSpan barrierSpan            = mImageRenderPassBarriers[passSpanIndex];
		uint32_t    beforePassBarrierCount = barrierSpan.BeforePassEnd - barrierSpan.BeforePassBegin;
		uint32_t    afterPassBarrierCount  = barrierSpan.AfterPassEnd  - barrierSpan.AfterPassBegin;

		if (beforePassBarrierCount != 0)
		{
			const VkImageMemoryBarrier*  imageBarrierPointer  = mImageBarriers.data() + barrierSpan.BeforePassBegin;
			const VkBufferMemoryBarrier* bufferBarrierPointer = nullptr;
			const VkMemoryBarrier*       memoryBarrierPointer = nullptr;

			vkCmdPipelineBarrier(graphicsCommandBuffer, barrierSpan.beforeFlagsBegin, barrierSpan.beforeFlagsEnd, 0, 0, memoryBarrierPointer, 0, bufferBarrierPointer, beforePassBarrierCount, imageBarrierPointer);
		}

		const RenderPassSpanInfo passSpanInfo = mPassFrameSpans[passSpanIndex];

		uint32_t renderPassFrame = passSpanInfo.OwnFrames * swapchainImageIndex + frameIndex % passSpanInfo.OwnFrames;
		uint32_t renderPassIndex = passSpanInfo.PassSpanBegin + renderPassFrame;
		mRenderPasses[renderPassIndex]->RecordExecution(graphicsCommandBuffer, scene, mFrameGraphConfig);

		if (afterPassBarrierCount != 0)
		{
			const VkImageMemoryBarrier*  imageBarrierPointer  = mImageBarriers.data() + barrierSpan.AfterPassBegin;
			const VkBufferMemoryBarrier* bufferBarrierPointer = nullptr;
			const VkMemoryBarrier*       memoryBarrierPointer = nullptr;

			vkCmdPipelineBarrier(graphicsCommandBuffer, barrierSpan.afterFlagsBegin, barrierSpan.AfterPassEnd, 0, 0, memoryBarrierPointer, 0, bufferBarrierPointer, beforePassBarrierCount, imageBarrierPointer);
		}
	}
}
