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

Vulkan::FrameGraph::FrameGraph(VkDevice device, FrameGraphConfig&& frameGraphConfig, const WorkerCommandBuffers* workerCommandBuffers, DeviceQueues* deviceQueues): ModernFrameGraph(std::move(frameGraphConfig)), mDeviceRef(device), mCommandBuffersRef(workerCommandBuffers), mDeviceQueuesRef(deviceQueues)
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

	for(Span<uint32_t> ownedImageSpan: mOwnedImageSpans)
	{
		for(uint32_t imageIndex = ownedImageSpan.Begin; imageIndex < ownedImageSpan.End; imageIndex++)
		{
			SafeDestroyObject(vkDestroyImage, mDeviceRef, mImages[imageIndex]);
		}
	}

	for(size_t i = 0; i < Utils::InFlightFrameCount; i++)
	{
		SafeDestroyObject(vkDestroySemaphore, mDeviceRef, mAcquireSemaphores[i]);
		SafeDestroyObject(vkDestroySemaphore, mDeviceRef, mGraphicsSemaphores[i]);
		SafeDestroyObject(vkDestroySemaphore, mDeviceRef, mPresentSemaphores[i]);
	}

	SafeDestroyObject(vkFreeMemory, mDeviceRef, mImageMemory);
}

void Vulkan::FrameGraph::Traverse(ThreadPool* threadPool, RenderableScene* scene, SwapChain* swapchain, VkFence traverseFence, uint32_t frameIndex, uint32_t swapchainImageIndex, VkSemaphore preTraverseSemaphore, VkSemaphore* outPostTraverseSemaphore)
{
	uint32_t currentFrameResourceIndex = frameIndex % Utils::InFlightFrameCount;
	VkSemaphore lastTraverseSemaphore = preTraverseSemaphore;

	BarrierPassSpan presentAcquirePassBarrierSpan = mRenderPassBarriers[mRenderPassBarriers.size() - SwapChain::SwapchainImageCount + swapchainImageIndex];

	const bool hasAcquirePass    = (presentAcquirePassBarrierSpan.AfterPassBegin != presentAcquirePassBarrierSpan.AfterPassEnd);
	const bool hasGraphicsPasses = (mGraphicsPassSpansPerDependencyLevel.size() > 0);
	const bool hasPresentPass    = (presentAcquirePassBarrierSpan.BeforePassBegin != presentAcquirePassBarrierSpan.BeforePassBegin);

	const bool presentPassLast    = hasPresentPass;
	const bool graphicsPassesLast = hasGraphicsPasses && !presentPassLast;
	const bool acquirePassLast    = hasAcquirePass && !graphicsPassesLast;

	if(hasAcquirePass) //Swapchain acquire barrier exists
	{
		VkFence acquireFenceToSignal = nullptr;
		if(acquirePassLast)
		{
			acquireFenceToSignal = traverseFence;
		}

		VkCommandBuffer acquireCommandBuffer = mCommandBuffersRef->GetMainThreadAcquireCommandBuffer(currentFrameResourceIndex);
		VkCommandPool   acquireCommandPool   = mCommandBuffersRef->GetMainThreadAcquireCommandPool(currentFrameResourceIndex);

		BeginCommandBuffer(acquireCommandBuffer, acquireCommandPool);

		const VkImageMemoryBarrier*  imageBarrierPointer = mImageBarriers.data() + presentAcquirePassBarrierSpan.AfterPassBegin;
		const VkBufferMemoryBarrier* bufferBarrierPointer = nullptr;
		const VkMemoryBarrier*       memoryBarrierPointer = nullptr;

		uint32_t barrierCount = presentAcquirePassBarrierSpan.AfterPassEnd - presentAcquirePassBarrierSpan.AfterPassBegin;
		vkCmdPipelineBarrier(acquireCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, memoryBarrierPointer, 0, bufferBarrierPointer, barrierCount, imageBarrierPointer);

		EndCommandBuffer(acquireCommandBuffer);

		VkSemaphore signalSemaphore = mAcquireSemaphores[currentFrameResourceIndex];
		swapchain->PresentQueueSubmit(acquireCommandBuffer, lastTraverseSemaphore, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, signalSemaphore, acquireFenceToSignal);

		lastTraverseSemaphore = signalSemaphore;
	}	

	if(hasGraphicsPasses)
	{
		struct ExecuteParameters
		{
			const WorkerCommandBuffers* CommandBuffers;
			const FrameGraph*           FrameGraph;
			const RenderableScene*      Scene;

			uint32_t FrameIndex;
			uint32_t FrameResourceIndex;
			uint32_t SwapchainImageIndex;
		} 
		executeParameters = 
		{
			.CommandBuffers = mCommandBuffersRef,
			.FrameGraph     = this,
			.Scene          = scene,

			.FrameIndex          = frameIndex,
			.FrameResourceIndex  = currentFrameResourceIndex,
			.SwapchainImageIndex = swapchainImageIndex
		};

		std::latch graphicsPassLatch((uint32_t)(mGraphicsPassSpansPerDependencyLevel.size() - 1));
		for(size_t dependencyLevelSpanIndex = 0; dependencyLevelSpanIndex < mGraphicsPassSpansPerDependencyLevel.size() - 1; dependencyLevelSpanIndex++)
		{
			struct JobData
			{
				ExecuteParameters* ExecuteParams;
				std::latch*        Waitable;
				uint32_t           DependencyLevelSpanIndex;
			}
			jobData =
			{
				.ExecuteParams            = &executeParameters,
				.Waitable                 = &graphicsPassLatch,
				.DependencyLevelSpanIndex = (uint32_t)dependencyLevelSpanIndex
			};

			auto executePassJob = [](void* userData, [[maybe_unused]] uint32_t userDataSize)
			{
				JobData* threadJobData = reinterpret_cast<JobData*>(userData);
				const FrameGraph* that = threadJobData->ExecuteParams->FrameGraph;

				VkCommandBuffer graphicsCommandBuffer = threadJobData->ExecuteParams->CommandBuffers->GetThreadGraphicsCommandBuffer(threadJobData->DependencyLevelSpanIndex, threadJobData->ExecuteParams->FrameResourceIndex);
				VkCommandPool   graphicsCommandPool   = threadJobData->ExecuteParams->CommandBuffers->GetThreadGraphicsCommandPool(threadJobData->DependencyLevelSpanIndex,   threadJobData->ExecuteParams->FrameResourceIndex);

				that->BeginCommandBuffer(graphicsCommandBuffer, graphicsCommandPool);
				that->RecordGraphicsPasses(graphicsCommandBuffer, threadJobData->ExecuteParams->Scene, threadJobData->DependencyLevelSpanIndex, threadJobData->ExecuteParams->FrameIndex, threadJobData->ExecuteParams->SwapchainImageIndex);
				that->EndCommandBuffer(graphicsCommandBuffer);

				threadJobData->Waitable->count_down();
			};

			threadPool->EnqueueWork(executePassJob, &jobData, sizeof(JobData));
		}

		VkCommandBuffer mainGraphicsCommandBuffer = mCommandBuffersRef->GetMainThreadGraphicsCommandBuffer(currentFrameResourceIndex);
		VkCommandPool   mainGraphicsCommandPool   = mCommandBuffersRef->GetMainThreadGraphicsCommandPool(currentFrameResourceIndex);
		
		BeginCommandBuffer(mainGraphicsCommandBuffer, mainGraphicsCommandPool);
		RecordGraphicsPasses(mainGraphicsCommandBuffer, scene, (uint32_t)(mGraphicsPassSpansPerDependencyLevel.size() - 1), frameIndex, swapchainImageIndex);
		EndCommandBuffer(mainGraphicsCommandBuffer);

		graphicsPassLatch.wait();

		for(size_t i = 0; i < mGraphicsPassSpansPerDependencyLevel.size() - 1; i++)
		{
			mFrameRecordedGraphicsCommandBuffers[i] = mCommandBuffersRef->GetThreadGraphicsCommandBuffer((uint32_t)i, currentFrameResourceIndex); //The command buffer that was used to record the command
		}

		VkFence graphicsFenceToSignal = nullptr;
		if(graphicsPassesLast)
		{
			graphicsFenceToSignal = traverseFence;
		}

		VkSemaphore graphicsSemaphore = mGraphicsSemaphores[currentFrameResourceIndex];
		mFrameRecordedGraphicsCommandBuffers[mGraphicsPassSpansPerDependencyLevel.size() - 1] = mainGraphicsCommandBuffer;
		mDeviceQueuesRef->GraphicsQueueSubmit(mFrameRecordedGraphicsCommandBuffers.data(), mGraphicsPassSpansPerDependencyLevel.size(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, lastTraverseSemaphore, graphicsSemaphore, graphicsFenceToSignal);

		lastTraverseSemaphore = graphicsSemaphore;
	}

	if(hasPresentPass) //Swapchain present barrier exists
	{
		VkFence presentFenceToSignal = traverseFence;

		VkCommandBuffer presentCommandBuffer = mCommandBuffersRef->GetMainThreadPresentCommandBuffer(currentFrameResourceIndex);
		VkCommandPool   presentCommandPool   = mCommandBuffersRef->GetMainThreadPresentCommandPool(currentFrameResourceIndex);

		BeginCommandBuffer(presentCommandBuffer, presentCommandPool);

		const VkImageMemoryBarrier*  imageBarrierPointer  = mImageBarriers.data() + presentAcquirePassBarrierSpan.BeforePassBegin;
		const VkBufferMemoryBarrier* bufferBarrierPointer = nullptr;
		const VkMemoryBarrier*       memoryBarrierPointer = nullptr;

		uint32_t barrierCount = presentAcquirePassBarrierSpan.BeforePassBegin - presentAcquirePassBarrierSpan.BeforePassEnd;
		vkCmdPipelineBarrier(presentCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, memoryBarrierPointer, 0, bufferBarrierPointer, barrierCount, imageBarrierPointer);

		EndCommandBuffer(presentCommandBuffer);

		VkSemaphore signalSemaphore = mPresentSemaphores[currentFrameResourceIndex];
		swapchain->PresentQueueSubmit(presentCommandBuffer, lastTraverseSemaphore, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, signalSemaphore, presentFenceToSignal);

		lastTraverseSemaphore = signalSemaphore;
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

		ThrowIfFailed(vkCreateSemaphore(mDeviceRef, &semaphoreCreateInfo, nullptr, &mAcquireSemaphores[i]));
		ThrowIfFailed(vkCreateSemaphore(mDeviceRef, &semaphoreCreateInfo, nullptr, &mGraphicsSemaphores[i]));
		ThrowIfFailed(vkCreateSemaphore(mDeviceRef, &semaphoreCreateInfo, nullptr, &mPresentSemaphores[i]));
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
	Span<uint32_t> levelSpan = mGraphicsPassSpansPerDependencyLevel[dependencyLevelSpanIndex];
	for(uint32_t passSpanIndex = levelSpan.Begin; passSpanIndex < levelSpan.End; passSpanIndex++)
	{
		uint32_t passIndex = CalcPassIndex(mFrameSpansPerRenderPass[passSpanIndex], frameIndex, swapchainImageIndex);

		const BarrierPassSpan& barrierSpan            = mRenderPassBarriers[passIndex];
		uint32_t               beforePassBarrierCount = barrierSpan.BeforePassEnd - barrierSpan.BeforePassBegin;
		uint32_t               afterPassBarrierCount  = barrierSpan.AfterPassEnd  - barrierSpan.AfterPassBegin;

		if(beforePassBarrierCount != 0)
		{
			const VkImageMemoryBarrier*  imageBarrierPointer  = mImageBarriers.data() + barrierSpan.BeforePassBegin;
			const VkBufferMemoryBarrier* bufferBarrierPointer = nullptr;
			const VkMemoryBarrier*       memoryBarrierPointer = nullptr;

			vkCmdPipelineBarrier(graphicsCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, memoryBarrierPointer, 0, bufferBarrierPointer, beforePassBarrierCount, imageBarrierPointer);
		}

		mRenderPasses[passIndex]->RecordExecution(graphicsCommandBuffer, scene, mFrameGraphConfig);

		if(afterPassBarrierCount != 0)
		{
			const VkImageMemoryBarrier*  imageBarrierPointer  = mImageBarriers.data() + barrierSpan.AfterPassBegin;
			const VkBufferMemoryBarrier* bufferBarrierPointer = nullptr;
			const VkMemoryBarrier*       memoryBarrierPointer = nullptr;

			vkCmdPipelineBarrier(graphicsCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, memoryBarrierPointer, 0, bufferBarrierPointer, beforePassBarrierCount, imageBarrierPointer);
		}
	}
}