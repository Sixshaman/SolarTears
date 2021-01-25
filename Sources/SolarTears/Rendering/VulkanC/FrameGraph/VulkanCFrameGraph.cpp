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

		VkCommandBuffer graphicsCommandBuffer = commandBuffers->GetMainThreadGraphicsCommandBuffer(currentFrameResourceIndex);
		VkCommandPool   graphicsCommandPool   = commandBuffers->GetMainThreadGraphicsCommandPool(currentFrameResourceIndex);
		BeginCommandBuffer(graphicsCommandBuffer, graphicsCommandPool);

		std::array<VkImageMemoryBarrier, 1> imageAcquireBarriers;
		imageAcquireBarriers[0].sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageAcquireBarriers[0].pNext                           = nullptr;
		imageAcquireBarriers[0].srcAccessMask                   = 0;
		imageAcquireBarriers[0].dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageAcquireBarriers[0].oldLayout                       = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		imageAcquireBarriers[0].newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageAcquireBarriers[0].srcQueueFamilyIndex             = swapChain->GetPresentQueueFamilyIndex();
		imageAcquireBarriers[0].dstQueueFamilyIndex             = deviceQueues->GetGraphicsQueueFamilyIndex();
		imageAcquireBarriers[0].image                           = mImages[mBackbufferRefIndex];
		imageAcquireBarriers[0].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		imageAcquireBarriers[0].subresourceRange.baseMipLevel   = 0;
		imageAcquireBarriers[0].subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
		imageAcquireBarriers[0].subresourceRange.baseArrayLayer = 0;
		imageAcquireBarriers[0].subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

		std::array<VkMemoryBarrier, 0>       memoryAcquireBarriers;
		std::array<VkBufferMemoryBarrier, 0> bufferAcquireBarriers;
		vkCmdPipelineBarrier(graphicsCommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
							 (uint32_t)memoryAcquireBarriers.size(), memoryAcquireBarriers.data(),
							 (uint32_t)bufferAcquireBarriers.size(), bufferAcquireBarriers.data(),
							 (uint32_t)imageAcquireBarriers.size(),   imageAcquireBarriers.data());

		while(renderPassIndex < mGraphicsPassCount)
		{
			mRenderPasses[renderPassIndex]->RecordExecution(graphicsCommandBuffer, scene, mFrameGraphConfig, currentFrameResourceIndex);

			renderPassIndex++;
		}

		std::array<VkImageMemoryBarrier, 1> imagePresentBarriers;
		imagePresentBarriers[0].sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imagePresentBarriers[0].pNext                           = nullptr;
		imagePresentBarriers[0].srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
		imagePresentBarriers[0].dstAccessMask                   = 0;
		imagePresentBarriers[0].oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imagePresentBarriers[0].newLayout                       = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		imagePresentBarriers[0].srcQueueFamilyIndex             = deviceQueues->GetGraphicsQueueFamilyIndex();
		imagePresentBarriers[0].dstQueueFamilyIndex             = swapChain->GetPresentQueueFamilyIndex();
		imagePresentBarriers[0].image                           = mImages[mBackbufferRefIndex];
		imagePresentBarriers[0].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		imagePresentBarriers[0].subresourceRange.baseMipLevel   = 0;
		imagePresentBarriers[0].subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
		imagePresentBarriers[0].subresourceRange.baseArrayLayer = 0;
		imagePresentBarriers[0].subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

		std::array<VkMemoryBarrier, 0>       memoryPresentBarriers;
		std::array<VkBufferMemoryBarrier, 0> bufferPresentBarriers;
		vkCmdPipelineBarrier(graphicsCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0,
							 (uint32_t)memoryPresentBarriers.size(), memoryPresentBarriers.data(),
							 (uint32_t)bufferPresentBarriers.size(), bufferPresentBarriers.data(),
							 (uint32_t)imagePresentBarriers.size(), imagePresentBarriers.data());

		EndCommandBuffer(graphicsCommandBuffer);

		std::array graphicsCommandBuffers = {graphicsCommandBuffer};
		deviceQueues->GraphicsQueueSubmit(graphicsCommandBuffers.data(), graphicsCommandBuffers.size(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, acquireSemaphore, graphicsSemaphore, nullptr);

		lastSemaphore = graphicsSemaphore;
	}

	if(mComputePassCount > 0)
	{
		VkSemaphore computeSemaphore = mComputeToPresentSemaphores[currentFrameResourceIndex];

		VkCommandBuffer computeCommandBuffer = commandBuffers->GetMainThreadComputeCommandBuffer(currentFrameResourceIndex);
		VkCommandPool   computeCommandPool   = commandBuffers->GetMainThreadComputeCommandPool(currentFrameResourceIndex);
		BeginCommandBuffer(computeCommandBuffer, computeCommandPool);

		while ((renderPassIndex - mGraphicsPassCount) < mComputePassCount)
		{
			mRenderPasses[renderPassIndex]->RecordExecution(computeCommandBuffer, scene, mFrameGraphConfig, currentFrameResourceIndex);

			renderPassIndex++;
		}

		EndCommandBuffer(computeCommandBuffer);

		std::array computeCommandBuffers = { computeCommandBuffer };
		deviceQueues->ComputeQueueSubmit(computeCommandBuffers.data(), computeCommandBuffers.size(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, lastSemaphore, computeSemaphore, traverseFence);

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