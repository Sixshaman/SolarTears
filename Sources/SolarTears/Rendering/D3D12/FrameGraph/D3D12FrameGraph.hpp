#pragma once

#include <d3d12.h>
#include <vector>
#include "D3D12RenderPass.hpp"
#include "../../FrameGraphConfig.hpp"

class ThreadPool;

namespace D3D12
{
	class WorkerCommandLists;
	class RenderableScene;
	class SwapChain;
	class DeviceQueues;

	class FrameGraph
	{
		friend class FrameGraphDescriptorCreator;
		friend class FrameGraphBuilder;

		struct BarrierSpan
		{
			uint32_t BeforePassBegin;
			uint32_t BeforePassEnd;
			uint32_t AfterPassBegin;
			uint32_t AfterPassEnd;
		};

		struct DependencyLevelSpan
		{
			uint32_t DependencyLevelBegin;
			uint32_t DependencyLevelEnd;
		};

	public:
		FrameGraph(const FrameGraphConfig& frameGraphConfig);
		~FrameGraph();

		void Traverse(ThreadPool* threadPool, WorkerCommandLists* commandLists, RenderableScene* scene, SwapChain* swapChain, DeviceQueues* deviceQueues, uint32_t currentFrameResourceIndex);

	private:
		void BeginCommandList(ID3D12GraphicsCommandList* commandList, ID3D12CommandAllocator* commandAllocator, uint32_t dependencyLevelSpanIndex) const;
		void RecordGraphicsPasses(ID3D12GraphicsCommandList6* commandList, const RenderableScene* scene, uint32_t dependencyLevelSpanIndex)        const;
		void EndCommandList(ID3D12GraphicsCommandList* commandList)                                                                                const;

	private:
		FrameGraphConfig mFrameGraphConfig;

		std::vector<std::unique_ptr<RenderPass>> mRenderPasses; //All currently used render passes (sorted by dependency level)

		std::vector<DependencyLevelSpan> mGraphicsPassSpans;

		std::vector<wil::com_ptr_nothrow<ID3D12Resource2>> mTextures;

		//Swapchain images and related data
		uint32_t mBackbufferRefIndex;

		std::vector<D3D12_RESOURCE_BARRIER> mResourceBarriers;
		std::vector<uint32_t>               mSwapchainBarrierIndices;
		std::vector<BarrierSpan>            mRenderPassBarriers; //Required barriers before ith pass are mResourceBarriers[Span.Begin...Span.End], where Span is mRenderPassBarriers[i]. Last span is for after-graph barriers

		//Used to track the command buffers that were used to record render passes
		std::vector<ID3D12CommandList*> mFrameRecordedGraphicsCommandLists;
	};
}