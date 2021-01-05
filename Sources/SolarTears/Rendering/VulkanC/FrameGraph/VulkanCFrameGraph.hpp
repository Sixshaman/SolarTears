#pragma once

#include <vector>
#include <memory>
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

		void Traverse(WorkerCommandBuffers* commandBuffers, RenderableScene* scene, ThreadPool* threadPool, uint32_t currentFrameResourceIndex);

	private:
		const VkDevice mDeviceRef;

		FrameGraphConfig mFrameGraphConfig;
	
		uint32_t mGraphicsPassCount;
		uint32_t mComputePassCount;
		uint32_t mTransferPassCount;

		uint32_t mGraphicsQueueFamilyIndex;
		uint32_t mComputeQueueFamilyIndex;
		uint32_t mTransferQueueFamilyIndex;

		std::vector<std::unique_ptr<RenderPass>> mRenderPasses;

		std::vector<VkImage>     mImages;
		std::vector<VkImageView> mImageViews;

		VkDeviceMemory mImageMemory;
	};
}