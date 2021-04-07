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
	class SwapChain;
	class DeviceQueues;

	class FrameGraph
	{
		friend class FrameGraphBuilder;
		
		struct BarrierSpan
		{
			uint32_t BeforePassBegin;
			uint32_t BeforePassEnd;
			uint32_t AfterPassBegin;
			uint32_t AfterPassEnd;

			VkPipelineStageFlags beforeFlagsBegin;
			VkPipelineStageFlags beforeFlagsEnd;

			VkPipelineStageFlags afterFlagsBegin;
			VkPipelineStageFlags afterFlagsEnd;
		};

		struct DependencyLevelSpan
		{
			uint32_t DependencyLevelBegin;
			uint32_t DependencyLevelEnd;
		};

		struct ThreadCommandInfo
		{
			VkCommandBuffer CommandBuffer;
			uint32_t        DependencyLevelSpanIndex;
		};

	public:
		FrameGraph(VkDevice device, uint32_t maxWorkerThreads, const FrameGraphConfig& frameGraphConfig);
		~FrameGraph();

		void Traverse(ThreadPool* threadPool, WorkerCommandBuffers* commandBuffers, RenderableScene* scene, SwapChain* swapChain, DeviceQueues* deviceQueues, VkFence traverseFence, uint32_t currentFrameResourceIndex, uint32_t currentSwapchainImageIndex);

	private:
		void CreateSemaphores();
		
		void SwitchSwapchainPasses(uint32_t swapchainImageIndex);
		void SwitchSwapchainImages(uint32_t swapchainImageIndex);

		void UnsetSwapchainPasses();
		void UnsetSwapchainImages();

		void BeginCommandBuffer(VkCommandBuffer cmdBuffer, VkCommandPool cmdPool) const;
		void EndCommandBuffer(VkCommandBuffer cmdBuffer)                          const;

		void RecordGraphicsPasses(VkCommandBuffer graphicsCommandBuffer, VkCommandPool graphicsCommandPool, const RenderableScene* scene, uint32_t dependencyLevelSpanIndex) const;

	private:
		const VkDevice mDeviceRef;

		FrameGraphConfig mFrameGraphConfig;

		std::vector<std::unique_ptr<RenderPass>> mRenderPasses;           //All currently used render passes (sorted by dependency level)
		std::vector<std::unique_ptr<RenderPass>> mSwapchainRenderPasses;  //All render passes that use swapchain images (replaced every frame)

		std::vector<DependencyLevelSpan> mGraphicsPassSpans;

		std::vector<VkImage>     mImages;
		std::vector<VkImageView> mImageViews;

		//Swapchain images and related data
		uint32_t                 mBackbufferRefIndex;
		uint32_t                 mLastSwapchainImageIndex;
		std::vector<VkImage>     mSwapchainImageRefs;
		std::vector<VkImageView> mSwapchainImageViews;

		//Barriers and barrier metadata
		std::vector<VkImageMemoryBarrier> mImageBarriers;
		std::vector<uint32_t>             mSwapchainBarrierIndices;
		std::vector<BarrierSpan>          mImageRenderPassBarriers; //Required barriers before ith pass are mImageBarriers[Span.Begin...Span.End], where Span is mImageRenderPassBarriers[i]. Last span is for after-graph barriers

		//Each i * (SwapchainFrameCount + 1) + 0 element tells the index in mRenderPasses/mImageViews that should be replaced with swapchain-related element every frame. 
		//Pass to replace is taken from mSwapchainRenderPasses[i * (SwapchainFrameCount + 1) + currentSwapchainImageIndex + 1]
		//View to replace is taken from   mSwapchainImageViews[i * (SwapchainFrameCount + 1) + currentSwapchainImageIndex + 1]
		//The reason to use it is to make mRenderPasses always relate to passes actually used
		std::vector<uint32_t> mSwapchainPassesSwapMap;
		std::vector<uint32_t> mSwapchainViewsSwapMap;

		VkDeviceMemory mImageMemory;

		VkSemaphore mGraphicsToPresentSemaphores[VulkanUtils::InFlightFrameCount];

		//Used to track the command buffers that were used to record render passes
		std::vector<std::unique_ptr<ThreadCommandInfo>> mTrackedCommandBuffers;
		std::vector<VkCommandBuffer>                    mFrameRecordedGraphicsCommandBuffers;
	};
}