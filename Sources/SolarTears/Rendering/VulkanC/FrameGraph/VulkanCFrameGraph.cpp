#include "VulkanCFrameGraph.hpp"
#include "../VulkanCWorkerCommandBuffers.hpp"
#include "../VulkanCFunctions.hpp"
#include "../VulkanCSwapChain.hpp"
#include "../VulkanCDeviceQueues.hpp"
#include "../../../Core/ThreadPool.hpp"
#include "../../../Core/WaitableObject.hpp"
#include <array>

VulkanCBindings::FrameGraph::FrameGraph(VkDevice device, const FrameGraphConfig& frameGraphConfig): mDeviceRef(device)
{
	mImageMemory = VK_NULL_HANDLE;

	mLastSwapchainImageIndex = 0;

	mFrameGraphConfig = frameGraphConfig;

	CreateSemaphores();
}

VulkanCBindings::FrameGraph::~FrameGraph()
{
	UnsetSwapchainPasses();
	UnsetSwapchainImages();

	for(size_t i = 0; i < mSwapchainImageViews.size(); i++)
	{
		SafeDestroyObject(vkDestroyImageView, mDeviceRef, mSwapchainImageViews[i]);
	}

	for(size_t i = 0; i < mImageViews.size(); i++)
	{
		SafeDestroyObject(vkDestroyImageView, mDeviceRef, mImageViews[i]);
	}

	for(size_t i = 0; i < mImages.size(); i++)
	{
		SafeDestroyObject(vkDestroyImage, mDeviceRef, mImages[i]);
	}

	for(size_t i = 0; i < VulkanUtils::InFlightFrameCount; i++)
	{
		SafeDestroyObject(vkDestroySemaphore, mDeviceRef, mGraphicsToPresentSemaphores[i]);
	}

	SafeDestroyObject(vkFreeMemory, mDeviceRef, mImageMemory);
}

void VulkanCBindings::FrameGraph::Traverse(ThreadPool* threadPool, WorkerCommandBuffers* commandBuffers, RenderableScene* scene, SwapChain* swapChain, DeviceQueues* deviceQueues, VkFence traverseFence, uint32_t currentFrameResourceIndex, uint32_t currentSwapchainImageIndex)
{
	SwitchSwapchainPasses(currentSwapchainImageIndex);
	SwitchSwapchainImages(currentSwapchainImageIndex);

	VkSemaphore acquireSemaphore = swapChain->GetImageAcquiredSemaphore(currentFrameResourceIndex);
	swapChain->AcquireImage(mDeviceRef, currentSwapchainImageIndex);

	VkSemaphore lastSemaphore = acquireSemaphore;
	if(mGraphicsPassSpans.size() > 0)
	{
		VkSemaphore graphicsSemaphore = mGraphicsToPresentSemaphores[currentFrameResourceIndex];
		VkFence     graphicsFence     = traverseFence; //Signal the fence after graphics command submission if there are no compute passes afterwards (which is always true for now)

		WaitableObject graphicsPassWriteWaitable((uint32_t)(mGraphicsPassSpans.size() - 1));
		for(size_t dependencyLevelSpanIndex = 0; dependencyLevelSpanIndex < mGraphicsPassSpans.size() - 1; dependencyLevelSpanIndex++)
		{
			struct JobData
			{
				const WorkerCommandBuffers*             CommandBuffers;
				const FrameGraph*                       FrameGraph;
				const VulkanCBindings::RenderableScene* Scene;
				WaitableObject*                         Waitable;
				uint32_t                                DependencyLevelSpanIndex;
				uint32_t                                FrameResourceIndex;
			}
			jobData =
			{
				.CommandBuffers           = commandBuffers,
				.FrameGraph               = this,
				.Scene                    = scene,
				.Waitable                 = &graphicsPassWriteWaitable,
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

				threadJobData->Waitable->NotifyJobFinished();
			};

			threadPool->EnqueueWork(executePassJob, &jobData, sizeof(JobData));
		}

		VkCommandBuffer mainGraphicsCommandBuffer = commandBuffers->GetMainThreadGraphicsCommandBuffer(currentFrameResourceIndex);
		VkCommandPool   mainGraphicsCommandPool   = commandBuffers->GetMainThreadGraphicsCommandPool(currentFrameResourceIndex);
		
		BeginCommandBuffer(mainGraphicsCommandBuffer, mainGraphicsCommandPool);
		RecordGraphicsPasses(mainGraphicsCommandBuffer, scene, (uint32_t)(mGraphicsPassSpans.size() - 1));
		EndCommandBuffer(mainGraphicsCommandBuffer);

		graphicsPassWriteWaitable.WaitForAll();

		for(size_t i = 0; i < mGraphicsPassSpans.size() - 1; i++)
		{
			mFrameRecordedGraphicsCommandBuffers[i] = commandBuffers->GetThreadGraphicsCommandBuffer((uint32_t)i, currentFrameResourceIndex); //The command buffer that was used to record the command
		}

		mFrameRecordedGraphicsCommandBuffers[mGraphicsPassSpans.size() - 1] = mainGraphicsCommandBuffer;
		deviceQueues->GraphicsQueueSubmit(mFrameRecordedGraphicsCommandBuffers.data(), mGraphicsPassSpans.size(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, acquireSemaphore, graphicsSemaphore, graphicsFence);

		lastSemaphore = graphicsSemaphore;
	}

	swapChain->Present(lastSemaphore);

	mLastSwapchainImageIndex = currentSwapchainImageIndex;
}

void VulkanCBindings::FrameGraph::CreateSemaphores()
{
	for(uint32_t i = 0; i < VulkanUtils::InFlightFrameCount; i++)
	{
		VkSemaphoreCreateInfo semaphoreCreateInfo;
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphoreCreateInfo.pNext = nullptr;
		semaphoreCreateInfo.flags = 0;

		ThrowIfFailed(vkCreateSemaphore(mDeviceRef, &semaphoreCreateInfo, nullptr, &mGraphicsToPresentSemaphores[i]));
	}
}

void VulkanCBindings::FrameGraph::SwitchSwapchainPasses(uint32_t swapchainImageIndex)
{
	uint32_t swapchainImageCount = (uint32_t)mSwapchainImageRefs.size();

	for(uint32_t i = 0; i < (uint32_t)mSwapchainPassesSwapMap.size(); i += (swapchainImageCount + 1u))
	{
		uint32_t passIndexToSwitch = mSwapchainPassesSwapMap[i];
		
		mRenderPasses[passIndexToSwitch].swap(mSwapchainRenderPasses[mSwapchainPassesSwapMap[i + mLastSwapchainImageIndex + 1]]);
		mRenderPasses[passIndexToSwitch].swap(mSwapchainRenderPasses[mSwapchainPassesSwapMap[i + swapchainImageIndex      + 1]]);
	}
}

void VulkanCBindings::FrameGraph::SwitchSwapchainImages(uint32_t swapchainImageIndex)
{
	//Swap images
	std::swap(mImages[mBackbufferRefIndex], mSwapchainImageRefs[mLastSwapchainImageIndex]);
	std::swap(mImages[mBackbufferRefIndex], mSwapchainImageRefs[swapchainImageIndex]);


	//Update barriers
	for(size_t i = 0; i < mSwapchainBarrierIndices.size(); i++)
	{
		mImageBarriers[i].image = mImages[mBackbufferRefIndex];
	}


	//Swap image views
	uint32_t swapchainImageViewCount = (uint32_t)mSwapchainImageViews.size();
	for(uint32_t i = 0; i < (uint32_t)mSwapchainViewsSwapMap.size(); i += (swapchainImageViewCount + 1u))
	{
		uint32_t viewIndexToSwitch = mSwapchainViewsSwapMap[i];
		
		std::swap(mImageViews[viewIndexToSwitch], mSwapchainImageViews[mSwapchainViewsSwapMap[i + mLastSwapchainImageIndex + 1]]);
		std::swap(mImageViews[viewIndexToSwitch], mSwapchainImageViews[mSwapchainViewsSwapMap[i + swapchainImageIndex + 1]]);
	}
}

void VulkanCBindings::FrameGraph::UnsetSwapchainPasses()
{
	uint32_t swapchainImageCount = (uint32_t)mSwapchainImageRefs.size();
	for(uint32_t i = 0; i < (uint32_t)mSwapchainPassesSwapMap.size(); i += (swapchainImageCount + 1u))
	{
		uint32_t passIndexToSwitch = mSwapchainPassesSwapMap[i];
		mRenderPasses[passIndexToSwitch].swap(mSwapchainRenderPasses[mSwapchainPassesSwapMap[i + mLastSwapchainImageIndex + 1]]);
	}
}

void VulkanCBindings::FrameGraph::UnsetSwapchainImages()
{
	//Swap images
	std::swap(mImages[mBackbufferRefIndex], mSwapchainImageRefs[mLastSwapchainImageIndex]);

	//Update barriers
	for(size_t i = 0; i < mSwapchainBarrierIndices.size(); i++)
	{
		mImageBarriers[i].image = mImages[mBackbufferRefIndex];
	}

	//Swap image views
	uint32_t swapchainImageViewCount = (uint32_t)mSwapchainImageViews.size();
	for(uint32_t i = 0; i < (uint32_t)mSwapchainViewsSwapMap.size(); i += (swapchainImageViewCount + 1u))
	{
		uint32_t viewIndexToSwitch = mSwapchainViewsSwapMap[i];
		std::swap(mImageViews[viewIndexToSwitch], mSwapchainImageViews[mSwapchainViewsSwapMap[i + mLastSwapchainImageIndex + 1]]);
	}
}

void VulkanCBindings::FrameGraph::BeginCommandBuffer(VkCommandBuffer cmdBuffer, VkCommandPool cmdPool) const
{
	ThrowIfFailed(vkResetCommandPool(mDeviceRef, cmdPool, 0));

	VkCommandBufferBeginInfo cmdBufferBeginInfo;
	cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufferBeginInfo.pNext = nullptr;
	cmdBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	cmdBufferBeginInfo.pInheritanceInfo = nullptr;

	ThrowIfFailed(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo));
}

void VulkanCBindings::FrameGraph::EndCommandBuffer(VkCommandBuffer cmdBuffer) const
{
	ThrowIfFailed(vkEndCommandBuffer(cmdBuffer));
}

void VulkanCBindings::FrameGraph::RecordGraphicsPasses(VkCommandBuffer graphicsCommandBuffer, const RenderableScene* scene, uint32_t dependencyLevelSpanIndex) const
{
	DependencyLevelSpan levelSpan = mGraphicsPassSpans[dependencyLevelSpanIndex];
	for(uint32_t renderPassIndex = levelSpan.DependencyLevelBegin; renderPassIndex < levelSpan.DependencyLevelEnd; renderPassIndex++)
	{
		BarrierSpan barrierSpan            = mImageRenderPassBarriers[renderPassIndex];
		uint32_t    beforePassBarrierCount = barrierSpan.BeforePassEnd - barrierSpan.BeforePassBegin;
		uint32_t    afterPassBarrierCount  = barrierSpan.AfterPassEnd  - barrierSpan.AfterPassBegin;

		if (beforePassBarrierCount != 0)
		{
			const VkImageMemoryBarrier*  imageBarrierPointer  = mImageBarriers.data() + barrierSpan.BeforePassBegin;
			const VkBufferMemoryBarrier* bufferBarrierPointer = nullptr;
			const VkMemoryBarrier*       memoryBarrierPointer = nullptr;

			vkCmdPipelineBarrier(graphicsCommandBuffer, barrierSpan.beforeFlagsBegin, barrierSpan.beforeFlagsEnd, 0, 0, memoryBarrierPointer, 0, bufferBarrierPointer, beforePassBarrierCount, imageBarrierPointer);
		}

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
