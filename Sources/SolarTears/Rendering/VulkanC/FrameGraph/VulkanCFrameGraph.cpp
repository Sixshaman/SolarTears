#include "VulkanCFrameGraph.hpp"
#include "../VulkanCWorkerCommandBuffers.hpp"
#include "../VulkanCFunctions.hpp"
#include "../VulkanCSwapChain.hpp"
#include "../VulkanCDeviceQueues.hpp"
#include "../../../Core/ThreadPool.hpp"
#include <array>

VulkanCBindings::FrameGraph::FrameGraph(VkDevice device): mDeviceRef(device)
{
	mGraphicsPassCount = 0;
	mComputePassCount  = 0;

	mImageMemory = VK_NULL_HANDLE;

	mLastSwapchainImageIndex = 0;

	CreateSemaphores();
}

VulkanCBindings::FrameGraph::~FrameGraph()
{
}

void VulkanCBindings::FrameGraph::Traverse(WorkerCommandBuffers* commandBuffers, RenderableScene* scene, ThreadPool* threadPool, SwapChain* swapChain, DeviceQueues* deviceQueues, VkFence traverseFence, uint32_t currentFrameResourceIndex, uint32_t currentSwapchainImageIndex)
{
	//TODO: multithreading (thread pools are too hard for me rn, I have to level up my multithreading/concurrency skills)
	UNREFERENCED_PARAMETER(threadPool);

	SwitchSwapchainPasses(currentSwapchainImageIndex);
	SwitchSwapchainImages(currentSwapchainImageIndex);

	uint32_t renderPassIndex = 0;


	VkSemaphore acquireSemaphore = swapChain->GetImageAcquiredSemaphore(currentFrameResourceIndex);
	swapChain->AcquireImage(mDeviceRef, currentSwapchainImageIndex);

	VkSemaphore lastSemaphore = acquireSemaphore;
	if(mGraphicsPassCount > 0)
	{
		VkSemaphore graphicsSemaphore = mGraphicsToComputeSemaphores[currentFrameResourceIndex];

		VkFence graphicsFence = nullptr;
		if(mComputePassCount == 0)
		{
			graphicsFence = traverseFence; //Signal the fence after graphics command submission if there are no compute passes	
		}

		VkCommandBuffer graphicsCommandBuffer = commandBuffers->GetMainThreadGraphicsCommandBuffer(currentFrameResourceIndex);
		VkCommandPool   graphicsCommandPool   = commandBuffers->GetMainThreadGraphicsCommandPool(currentFrameResourceIndex);
		BeginCommandBuffer(graphicsCommandBuffer, graphicsCommandPool);

		while(renderPassIndex < mGraphicsPassCount)
		{
			BarrierSpan barrierSpan            = mImageRenderPassBarriers[renderPassIndex];
			uint32_t    beforePassBarrierCount = barrierSpan.BeforePassEnd - barrierSpan.BeforePassBegin;
			uint32_t    afterPassBarrierCount  = barrierSpan.AfterPassEnd  - barrierSpan.AfterPassBegin;

			if(beforePassBarrierCount != 0)
			{
				const VkImageMemoryBarrier*  imageBarrierPointer  = mImageBarriers.data() + barrierSpan.BeforePassBegin;
				const VkBufferMemoryBarrier* bufferBarrierPointer = nullptr;
				const VkMemoryBarrier*       memoryBarrierPointer = nullptr;

				vkCmdPipelineBarrier(graphicsCommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, memoryBarrierPointer, 0, bufferBarrierPointer, beforePassBarrierCount, imageBarrierPointer);
			}

			mRenderPasses[renderPassIndex]->RecordExecution(graphicsCommandBuffer, scene, mFrameGraphConfig, currentFrameResourceIndex);

			if(afterPassBarrierCount != 0)
			{
				const VkImageMemoryBarrier*  imageBarrierPointer  = mImageBarriers.data() + barrierSpan.AfterPassBegin;
				const VkBufferMemoryBarrier* bufferBarrierPointer = nullptr;
				const VkMemoryBarrier*       memoryBarrierPointer = nullptr;

				vkCmdPipelineBarrier(graphicsCommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, memoryBarrierPointer, 0, bufferBarrierPointer, beforePassBarrierCount, imageBarrierPointer);
			}

			renderPassIndex++;
		}

		EndCommandBuffer(graphicsCommandBuffer);

		std::array graphicsCommandBuffers = {graphicsCommandBuffer};
		deviceQueues->GraphicsQueueSubmit(graphicsCommandBuffers.data(), graphicsCommandBuffers.size(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, acquireSemaphore, graphicsSemaphore, graphicsFence);

		lastSemaphore = graphicsSemaphore;
	}

	if(mComputePassCount > 0)
	{
		VkSemaphore computeSemaphore = mComputeToPresentSemaphores[currentFrameResourceIndex];

		VkFence computeFence = traverseFence;

		VkCommandBuffer computeCommandBuffer = commandBuffers->GetMainThreadComputeCommandBuffer(currentFrameResourceIndex);
		VkCommandPool   computeCommandPool   = commandBuffers->GetMainThreadComputeCommandPool(currentFrameResourceIndex);
		BeginCommandBuffer(computeCommandBuffer, computeCommandPool);

		while((renderPassIndex - mGraphicsPassCount) < mComputePassCount)
		{
			BarrierSpan barrierSpan            = mImageRenderPassBarriers[renderPassIndex];
			uint32_t    beforePassBarrierCount = barrierSpan.BeforePassEnd - barrierSpan.BeforePassBegin;
			uint32_t    afterPassBarrierCount  = barrierSpan.AfterPassEnd  - barrierSpan.AfterPassBegin;

			if(beforePassBarrierCount != 0)
			{
				const VkImageMemoryBarrier*  imageBarrierPointer  = mImageBarriers.data() + barrierSpan.BeforePassBegin;
				const VkBufferMemoryBarrier* bufferBarrierPointer = nullptr;
				const VkMemoryBarrier*       memoryBarrierPointer = nullptr;

				vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, memoryBarrierPointer, 0, bufferBarrierPointer, beforePassBarrierCount, imageBarrierPointer);
			}

			mRenderPasses[renderPassIndex]->RecordExecution(computeCommandBuffer, scene, mFrameGraphConfig, currentFrameResourceIndex);

			if(afterPassBarrierCount != 0)
			{
				const VkImageMemoryBarrier*  imageBarrierPointer  = mImageBarriers.data() + barrierSpan.AfterPassBegin;
				const VkBufferMemoryBarrier* bufferBarrierPointer = nullptr;
				const VkMemoryBarrier*       memoryBarrierPointer = nullptr;

				vkCmdPipelineBarrier(computeCommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, memoryBarrierPointer, 0, bufferBarrierPointer, beforePassBarrierCount, imageBarrierPointer);
			}

			renderPassIndex++;
		}

		EndCommandBuffer(computeCommandBuffer);

		std::array computeCommandBuffers = { computeCommandBuffer };
		deviceQueues->ComputeQueueSubmit(computeCommandBuffers.data(), computeCommandBuffers.size(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, lastSemaphore, computeSemaphore, computeFence);

		lastSemaphore = computeSemaphore;
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

		ThrowIfFailed(vkCreateSemaphore(mDeviceRef, &semaphoreCreateInfo, nullptr, &mGraphicsToComputeSemaphores[i]));
		ThrowIfFailed(vkCreateSemaphore(mDeviceRef, &semaphoreCreateInfo, nullptr, &mComputeToPresentSemaphores[i]));
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
	std::swap(mImages[mBackbufferRefIndex], mSwapchainImageRefs[mLastSwapchainImageIndex]);
	std::swap(mImages[mBackbufferRefIndex], mSwapchainImageRefs[swapchainImageIndex]);

	uint32_t swapchainImageCount = (uint32_t)mSwapchainImageRefs.size();

	for(uint32_t i = 0; i < (uint32_t)mSwapchainViewsSwapMap.size(); i += (swapchainImageCount + 1u))
	{
		uint32_t viewIndexToSwitch = mSwapchainViewsSwapMap[i];
		
		std::swap(mImageViews[viewIndexToSwitch], mSwapchainImageViews[mSwapchainViewsSwapMap[i + mLastSwapchainImageIndex + 1]]);
		std::swap(mImageViews[viewIndexToSwitch], mSwapchainImageViews[mSwapchainViewsSwapMap[i + swapchainImageIndex + 1]]);
	}
}

void VulkanCBindings::FrameGraph::BeginCommandBuffer(VkCommandBuffer cmdBuffer, VkCommandPool cmdPool)
{
	ThrowIfFailed(vkResetCommandPool(mDeviceRef, cmdPool, 0));

	VkCommandBufferBeginInfo cmdBufferBeginInfo;
	cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufferBeginInfo.pNext = nullptr;
	cmdBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	cmdBufferBeginInfo.pInheritanceInfo = nullptr;

	ThrowIfFailed(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo));
}

void VulkanCBindings::FrameGraph::EndCommandBuffer(VkCommandBuffer cmdBuffer)
{
	ThrowIfFailed(vkEndCommandBuffer(cmdBuffer));
}