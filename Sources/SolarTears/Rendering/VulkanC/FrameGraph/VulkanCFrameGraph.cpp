#include "VulkanCFrameGraph.hpp"
#include "../VulkanCWorkerCommandBuffers.hpp"
#include "../../../Core/ThreadPool.hpp"

VulkanCBindings::FrameGraph::FrameGraph(VkDevice device, uint32_t graphicsQueueIndex, uint32_t computeQueueIndex, uint32_t transferQueueIndex): mDeviceRef(device), mGraphicsQueueFamilyIndex(graphicsQueueIndex), mComputeQueueFamilyIndex(computeQueueIndex), mTransferQueueFamilyIndex(transferQueueIndex)
{
	mGraphicsPassCount = 0;
	mComputePassCount  = 0;
	mTransferPassCount = 0;

	mImageMemory = VK_NULL_HANDLE;

	mLastSwapchainImageIndex = 0;
}

VulkanCBindings::FrameGraph::~FrameGraph()
{
}

void VulkanCBindings::FrameGraph::Traverse(WorkerCommandBuffers* commandBuffers, RenderableScene* scene, ThreadPool* threadPool, uint32_t currentFrameResourceIndex, uint32_t currentSwapchainImageIndex)
{
	//TODO: multithreading (thread pools are too hard for me rn, I have to level up my multithreading/concurrency skills)
	UNREFERENCED_PARAMETER(threadPool);

	SwitchSwapchainPasses(currentSwapchainImageIndex);
	SwitchSwapchainImages(currentSwapchainImageIndex);

	uint32_t renderPassIndex = 0;

	VkCommandBuffer graphicsCommandBuffer = commandBuffers->GetMainThreadGraphicsCommandBuffer(currentFrameResourceIndex);
	while(renderPassIndex < mGraphicsPassCount)
	{
		mRenderPasses[renderPassIndex]->RecordExecution(graphicsCommandBuffer, scene, mFrameGraphConfig, currentFrameResourceIndex);

		renderPassIndex++;
	}

	VkCommandBuffer computeCommandBuffer = commandBuffers->GetMainThreadComputeCommandBuffer(currentFrameResourceIndex);
	while((renderPassIndex - mGraphicsPassCount) < mComputePassCount)
	{
		mRenderPasses[renderPassIndex]->RecordExecution(computeCommandBuffer, scene, mFrameGraphConfig, currentFrameResourceIndex);

		renderPassIndex++;
	}

	VkCommandBuffer transferCommandBuffer = commandBuffers->GetMainThreadTransferCommandBuffer(currentFrameResourceIndex);
	while((renderPassIndex - mGraphicsPassCount - mComputePassCount) < mTransferPassCount)
	{
		mRenderPasses[renderPassIndex]->RecordExecution(transferCommandBuffer, scene, mFrameGraphConfig, currentFrameResourceIndex);

		renderPassIndex++;
	}

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