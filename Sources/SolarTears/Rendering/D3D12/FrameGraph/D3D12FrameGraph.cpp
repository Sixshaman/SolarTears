#include "D3D12FrameGraph.hpp"
#include "../../../Core/ThreadPool.hpp"
#include "../Scene/D3D12Scene.hpp"
#include "../D3D12WorkerCommandLists.hpp"
#include "../D3D12SwapChain.hpp"
#include "../D3D12DeviceQueues.hpp"
#include <latch>
#include <wil/com.h>

D3D12::FrameGraph::FrameGraph(const FrameGraphConfig& frameGraphConfig): mFrameGraphConfig(frameGraphConfig)
{
}

D3D12::FrameGraph::~FrameGraph()
{
}

void D3D12::FrameGraph::Traverse(ThreadPool* threadPool, WorkerCommandLists* commandLists, RenderableScene* scene, SwapChain* swapChain, DeviceQueues* deviceQueues, uint32_t currentFrameResourceIndex)
{
	if(mGraphicsPassSpans.size() > 0)
	{
		std::latch graphicsPassLatch((uint32_t)(mGraphicsPassSpans.size() - 1));
		for(size_t dependencyLevelSpanIndex = 0; dependencyLevelSpanIndex < mGraphicsPassSpans.size() - 1; dependencyLevelSpanIndex++)
		{
			struct JobData
			{
				const WorkerCommandLists*     CommandLists;
				const FrameGraph*             FrameGraph;
				const D3D12::RenderableScene* Scene;
				std::latch*                   Waitable;
				uint32_t                      DependencyLevelSpanIndex;
				uint32_t                      FrameResourceIndex;
			}
			jobData =
			{
				.CommandLists             = commandLists,
				.FrameGraph               = this,
				.Scene                    = scene,
				.Waitable                 = &graphicsPassLatch,
				.DependencyLevelSpanIndex = (uint32_t)dependencyLevelSpanIndex,
				.FrameResourceIndex       = currentFrameResourceIndex
			};

			auto executePassJob = [](void* userData, uint32_t userDataSize)
			{
				UNREFERENCED_PARAMETER(userDataSize);

				JobData* threadJobData = reinterpret_cast<JobData*>(userData);
				const FrameGraph* that = threadJobData->FrameGraph;

				ID3D12GraphicsCommandList* graphicsCommandList      = threadJobData->CommandLists->GetThreadDirectCommandList(threadJobData->DependencyLevelSpanIndex);
				ID3D12CommandAllocator*    graphicsCommandAllocator = threadJobData->CommandLists->GetThreadDirectCommandAllocator(threadJobData->DependencyLevelSpanIndex, threadJobData->FrameResourceIndex);

				that->BeginCommandList(graphicsCommandList, graphicsCommandAllocator, threadJobData->DependencyLevelSpanIndex);
				that->RecordGraphicsPasses(graphicsCommandList, threadJobData->Scene, threadJobData->DependencyLevelSpanIndex);
				that->EndCommandList(graphicsCommandList);

				threadJobData->Waitable->count_down();
			};

			threadPool->EnqueueWork(executePassJob, &jobData, sizeof(JobData));
		}

		ID3D12GraphicsCommandList* mainGraphicsCommandList      = commandLists->GetMainThreadDirectCommandList();
		ID3D12CommandAllocator*    mainGraphicsCommandAllocator = commandLists->GetMainThreadDirectCommandAllocator(currentFrameResourceIndex);
		
		BeginCommandList(mainGraphicsCommandList, mainGraphicsCommandAllocator, (uint32_t)(mGraphicsPassSpans.size() - 1));
		RecordGraphicsPasses(mainGraphicsCommandList, scene, (uint32_t)(mGraphicsPassSpans.size() - 1));
		EndCommandList(mainGraphicsCommandList);

		graphicsPassLatch.wait();

		for(size_t i = 0; i < mGraphicsPassSpans.size() - 1; i++)
		{
			mFrameRecordedGraphicsCommandLists[i] = commandLists->GetThreadDirectCommandList((uint32_t)i); //The command buffer that was used to record the command
		}

		mFrameRecordedGraphicsCommandLists[mGraphicsPassSpans.size() - 1] = mainGraphicsCommandList;

		ID3D12CommandQueue* graphicsQueue = deviceQueues->GraphicsQueueHandle();
		graphicsQueue->ExecuteCommandLists((UINT)mGraphicsPassSpans.size(), mFrameRecordedGraphicsCommandLists.data());
	}

	swapChain->Present();
}

void D3D12::FrameGraph::BeginCommandList(ID3D12GraphicsCommandList* commandList, ID3D12CommandAllocator* commandAllocator, uint32_t dependencyLevelSpanIndex) const
{
	//TODO: Reset with the first pipeline statre in the render pass
	UNREFERENCED_PARAMETER(dependencyLevelSpanIndex);

	THROW_IF_FAILED(commandAllocator->Reset());
	THROW_IF_FAILED(commandList->Reset(commandAllocator, nullptr));
}

void D3D12::FrameGraph::EndCommandList(ID3D12GraphicsCommandList* commandList) const
{
	THROW_IF_FAILED(commandList->Close());
}

void D3D12::FrameGraph::RecordGraphicsPasses(ID3D12GraphicsCommandList* commandList, const RenderableScene* scene, uint32_t dependencyLevelSpanIndex) const
{
	UNREFERENCED_PARAMETER(commandList);
	UNREFERENCED_PARAMETER(scene);

	DependencyLevelSpan levelSpan = mGraphicsPassSpans[dependencyLevelSpanIndex];
	for(uint32_t renderPassIndex = levelSpan.DependencyLevelBegin; renderPassIndex < levelSpan.DependencyLevelEnd; renderPassIndex++)
	{
		
	}
}