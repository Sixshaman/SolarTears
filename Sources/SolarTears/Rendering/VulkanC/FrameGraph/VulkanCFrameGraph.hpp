#pragma once

#include <vector>
#include <memory>
#include <unordered_set>
#include "VulkanCRenderPass.hpp"
#include "VulkanCFrameGraphConfig.hpp"

class ThreadPool;

namespace VulkanCBindings
{
	class WorkerCommandBuffers;

	class FrameGraph
	{
		friend class FrameGraphBuilder;

	public:
		FrameGraph(VkDevice device, uint32_t graphicsQueueIndex, uint32_t computeQueueIndex, uint32_t transferQueueIndex);
		~FrameGraph();

		void Traverse(WorkerCommandBuffers* commandBuffers, RenderableScene* scene, ThreadPool* threadPool, uint32_t currentFrameResourceIndex, uint32_t currentSwapchainImageIndex);

	private:
		void SwitchSwapchainPasses(uint32_t swapchainImageIndex);
		void SwitchSwapchainImages(uint32_t swapchainImageIndex);

	private:
		const VkDevice mDeviceRef;

		FrameGraphConfig mFrameGraphConfig;
	
		uint32_t mGraphicsPassCount;
		uint32_t mComputePassCount;
		uint32_t mTransferPassCount;

		uint32_t mGraphicsQueueFamilyIndex;
		uint32_t mComputeQueueFamilyIndex;
		uint32_t mTransferQueueFamilyIndex;

		std::vector<std::unique_ptr<RenderPass>> mRenderPasses;           //All currently used render passes
		std::vector<std::unique_ptr<RenderPass>> mSwapchainRenderPasses;  //All render passes that use swapchain images (replaced every frame)

		std::vector<VkImage>     mImages;
		std::vector<VkImageView> mImageViews;

		uint32_t                 mBackbufferRefIndex;
		uint32_t                 mLastSwapchainImageIndex;
		std::vector<VkImage>     mSwapchainImageRefs;
		std::vector<VkImageView> mSwapchainImageViews;

		//Each i * (SwapchainFrameCount + 1) + 0 element tells the index in mRenderPasses/mImageViews that should be replaced with swapchain-related element every frame. 
		//Pass to replace is taken from mSwapchainRenderPasses[i * (SwapchainFrameCount + 1) + currentSwapchainImageIndex + 1]
		//View to replace is taken from   mSwapchainImageViews[i * (SwapchainFrameCount + 1) + currentSwapchainImageIndex + 1]
		std::vector<uint32_t> mSwapchainPassesSwapMap;
		std::vector<uint32_t> mSwapchainViewsSwapMap;

		VkDeviceMemory mImageMemory;
	};
}