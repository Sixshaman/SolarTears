#pragma once

#include <vector>
#include <memory>
#include <unordered_set>
#include "VulkanRenderPass.hpp"
#include "../../Common/RenderingUtils.hpp"
#include "../../Common/FrameGraph/FrameGraphConfig.hpp"
#include "../../Common/FrameGraph/ModernFrameGraph.hpp"

class ThreadPool;

namespace Vulkan
{
	class WorkerCommandBuffers;
	class SwapChain;
	class DeviceQueues;

	class FrameGraph: public ModernFrameGraph
	{
		friend class FrameGraphBuilder;

	public:
		FrameGraph(VkDevice device, FrameGraphConfig&& frameGraphConfig, const WorkerCommandBuffers* workerCommandBuffers, DeviceQueues* deviceQueues);
		~FrameGraph();

		void Traverse(ThreadPool* threadPool, RenderableScene* scene, SwapChain* swapchain, VkFence traverseFence, uint32_t frameIndex, uint32_t swapchainImageIndex, VkSemaphore preTraverseSemaphore, VkSemaphore* outPostTraverseSemaphore);

	private:
		void CreateSemaphores();

		void BeginCommandBuffer(VkCommandBuffer cmdBuffer, VkCommandPool cmdPool) const;
		void EndCommandBuffer(VkCommandBuffer cmdBuffer)                          const;

		void RecordGraphicsPasses(VkCommandBuffer graphicsCommandBuffer, const RenderableScene* scene, uint32_t dependencyLevelSpanIndex, uint32_t frameIndex, uint32_t swapchainImageIndex) const;

	private:
		const VkDevice mDeviceRef;

		const WorkerCommandBuffers* mCommandBuffersRef;
		const DeviceQueues*         mDeviceQueuesRef;

		std::vector<std::unique_ptr<RenderPass>> mRenderPasses; //All render passes

		std::vector<VkImage>     mImages;
		std::vector<VkImageView> mImageViews;
		VkDeviceMemory           mImageMemory;

		std::vector<VkImageMemoryBarrier> mImageBarriers;

		std::vector<VkDescriptorSet> mDescriptorSets;

		VkSemaphore mAcquireSemaphores[Utils::InFlightFrameCount];
		VkSemaphore mGraphicsSemaphores[Utils::InFlightFrameCount];
		VkSemaphore mPresentSemaphores[Utils::InFlightFrameCount];

		//Used to track the command buffers used to record the render passes
		std::vector<VkCommandBuffer> mFrameRecordedGraphicsCommandBuffers;

		std::vector<Span<uint32_t>> mOwnedImageSpans;
	};
}