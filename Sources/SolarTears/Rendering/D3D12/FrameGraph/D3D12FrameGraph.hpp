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
		void RecordGraphicsPasses(ID3D12GraphicsCommandList* commandList, const RenderableScene* scene, uint32_t dependencyLevelSpanIndex)         const;
		void EndCommandList(ID3D12GraphicsCommandList* commandList)                                                                                const;

	private:
		FrameGraphConfig mFrameGraphConfig;

		std::vector<std::unique_ptr<RenderPass>> mRenderPasses; //All currently used render passes (sorted by dependency level)

		std::vector<DependencyLevelSpan> mGraphicsPassSpans;

		//Used to track the command buffers that were used to record render passes
		std::vector<ID3D12CommandList*> mFrameRecordedGraphicsCommandLists;
	};
}