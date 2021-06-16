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

		void Traverse(ThreadPool* threadPool, WorkerCommandBuffers* commandBuffers, RenderableScene* scene, DeviceQueues* deviceQueues, VkFence traverseFence, uint32_t frameIndex, uint32_t swapchainImageIndex, VkSemaphore preTraverseSemaphore, VkSemaphore* outPostTraverseSemaphore);

	private:
		void CreateSemaphores();

		void SwitchSwapchainImages(uint32_t swapchainImageIndex);

		void BeginCommandBuffer(VkCommandBuffer cmdBuffer, VkCommandPool cmdPool) const;
		void EndCommandBuffer(VkCommandBuffer cmdBuffer)                          const;

		void RecordGraphicsPasses(VkCommandBuffer graphicsCommandBuffer, const RenderableScene* scene, uint32_t dependencyLevelSpanIndex, uint32_t frameIndex, uint32_t swapchainImageIndex) const;

	private:
		const VkDevice mDeviceRef;

		std::vector<std::unique_ptr<RenderPass>> mRenderPasses; //All render passes

		std::vector<VkImage>     mImages;
		std::vector<VkImageView> mImageViews;

		//Barriers and barrier metadata
		std::vector<VkImageMemoryBarrier> mImageBarriers;
		std::vector<uint32_t>             mSwapchainBarrierIndices;
		std::vector<BarrierSpan>          mImageRenderPassBarriers; //Required barriers before ith pass are mImageBarriers[Span.Begin...Span.End], where Span is mImageRenderPassBarriers[i]. Last span is for after-graph barriers

		VkDeviceMemory mImageMemory;

		VkSemaphore mGraphicsToPresentSemaphores[Utils::InFlightFrameCount];

		//Used to track the command buffers that were used to record render passes
		std::vector<VkCommandBuffer> mFrameRecordedGraphicsCommandBuffers;
	};
}