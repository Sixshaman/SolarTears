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

	VkSemaphore presentToGraphicsSemaphore = swapChain->GetImageAcquiredSemaphore(currentFrameResourceIndex);
	VkSemaphore graphicsToComputeSemaphore = mGraphicsToComputeSemaphores[currentFrameResourceIndex];
	VkSemaphore computeToPresentSemaphore  = mComputeToPresentSemaphores[currentFrameResourceIndex];

	swapChain->AcquireImage(mDeviceRef, currentSwapchainImageIndex);


	VkCommandBuffer graphicsCommandBuffer = commandBuffers->GetMainThreadGraphicsCommandBuffer(currentFrameResourceIndex);
	VkCommandPool   graphicsCommandPool   = commandBuffers->GetMainThreadGraphicsCommandPool(currentFrameResourceIndex);
	BeginCommandBuffer(graphicsCommandBuffer, graphicsCommandPool);

	while(renderPassIndex < mGraphicsPassCount)
	{
		mRenderPasses[renderPassIndex]->RecordExecution(graphicsCommandBuffer, scene, mFrameGraphConfig, currentFrameResourceIndex);

		renderPassIndex++;
	}

	EndCommandBuffer(graphicsCommandBuffer);

	std::array graphicsCommandBuffers = {graphicsCommandBuffer};
	deviceQueues->GraphicsQueueSubmit(graphicsCommandBuffers.data(), graphicsCommandBuffers.size(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, presentToGraphicsSemaphore, graphicsToComputeSemaphore, nullptr);


	VkCommandBuffer computeCommandBuffer = commandBuffers->GetMainThreadComputeCommandBuffer(currentFrameResourceIndex);
	VkCommandPool   computeCommandPool   = commandBuffers->GetMainThreadComputeCommandPool(currentFrameResourceIndex);
	BeginCommandBuffer(computeCommandBuffer, computeCommandPool);

	while((renderPassIndex - mGraphicsPassCount) < mComputePassCount)
	{
		mRenderPasses[renderPassIndex]->RecordExecution(computeCommandBuffer, scene, mFrameGraphConfig, currentFrameResourceIndex);

		renderPassIndex++;
	}

	EndCommandBuffer(computeCommandBuffer);

	std::array computeCommandBuffers  = {computeCommandBuffer};
	deviceQueues->ComputeQueueSubmit(computeCommandBuffers.data(), computeCommandBuffers.size(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, graphicsToComputeSemaphore, computeToPresentSemaphore, traverseFence);


	swapChain->Present(computeToPresentSemaphore);

	mLastSwapchainImageIndex = currentSwapchainImageIndex;
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