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

	public:
		FrameGraph(VkDevice device, const FrameGraphConfig& frameGraphConfig);
		~FrameGraph();

		void Traverse(const ThreadPool* threadPool, WorkerCommandBuffers* commandBuffers, RenderableScene* scene, SwapChain* swapChain, DeviceQueues* deviceQueues, VkFence traverseFence, uint32_t currentFrameResourceIndex, uint32_t currentSwapchainImageIndex);

	private:
		void CreateSemaphores();
		
		void SwitchSwapchainPasses(uint32_t swapchainImageIndex);
		void SwitchSwapchainImages(uint32_t swapchainImageIndex);

		void UnsetSwapchainPasses();
		void UnsetSwapchainImages();

		void BeginCommandBuffer(VkCommandBuffer cmdBuffer, VkCommandPool cmdPool);
		void EndCommandBuffer(VkCommandBuffer cmdBuffer);

	private:
		const VkDevice mDeviceRef;

		FrameGraphConfig mFrameGraphConfig;
	
		uint32_t mGraphicsPassCount;
		uint32_t mComputePassCount;

		std::vector<std::unique_ptr<RenderPass>> mRenderPasses;           //All currently used render passes
		std::vector<std::unique_ptr<RenderPass>> mSwapchainRenderPasses;  //All render passes that use swapchain images (replaced every frame)

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

		VkSemaphore mGraphicsToComputeSemaphores[VulkanUtils::InFlightFrameCount];
		VkSemaphore mComputeToPresentSemaphores[VulkanUtils::InFlightFrameCount];
	};
}