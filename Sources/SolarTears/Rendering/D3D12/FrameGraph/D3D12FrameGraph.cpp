#include "D3D12FrameGraph.hpp"
#include "../../../Core/ThreadPool.hpp"
#include "../Scene/D3D12Scene.hpp"
#include "../D3D12WorkerCommandLists.hpp"
#include "../D3D12SwapChain.hpp"
#include "../D3D12DeviceQueues.hpp"
#include "../D3D12SrvDescriptorManager.hpp"
#include <latch>
#include <array>
#include <wil/com.h>

D3D12::FrameGraph::FrameGraph(FrameGraphConfig&& frameGraphConfig, const WorkerCommandLists* workerCommandLists, const SrvDescriptorManager* descriptorManager, DeviceQueues* deviceQueues): ModernFrameGraph(std::move(frameGraphConfig)),
                                                                                                                                                                                             mCommandListsRef(workerCommandLists),
	                                                                                                                                                                                         mDescriptorManagerRef(descriptorManager),
	                                                                                                                                                                                         mDeviceQueuesRef(deviceQueues)
{
	//We don't have any shader-visible descriptor heap yet. Until that we'll store all descriptors offseted from address 0
	mFrameGraphDescriptorStart = D3D12_GPU_DESCRIPTOR_HANDLE{.ptr = 0};
}

D3D12::FrameGraph::~FrameGraph()
{
}

void D3D12::FrameGraph::Traverse(ThreadPool* threadPool, const RenderableScene* scene, uint32_t frameIndex, uint32_t swapchainImageIndex)
{
	uint32_t currentFrameResourceIndex = frameIndex % Utils::InFlightFrameCount;
	if(mGraphicsPassSpansPerDependencyLevel.size() > 0)
	{
		struct ExecuteParameters
		{
			const FrameGraph*      FrameGraph;
			const RenderableScene* Scene;

			uint32_t FrameIndex;
			uint32_t FrameResourceIndex;
			uint32_t SwapchainImageIndex;
		} 
		executeParameters = 
		{
			.FrameGraph = this,
			.Scene      = scene,

			.FrameIndex          = frameIndex,
			.FrameResourceIndex  = currentFrameResourceIndex,
			.SwapchainImageIndex = swapchainImageIndex
		};

		std::latch graphicsPassLatch((uint32_t)(mGraphicsPassSpansPerDependencyLevel.size() - 1));
		for(size_t dependencyLevelSpanIndex = 0; dependencyLevelSpanIndex < mGraphicsPassSpansPerDependencyLevel.size() - 1; dependencyLevelSpanIndex++)
		{
			struct JobData
			{
				ExecuteParameters* ExecuteParams;
				std::latch*        Waitable;
				uint32_t           DependencyLevelSpanIndex;
			}
			jobData =
			{
				.ExecuteParams            = &executeParameters,
				.Waitable                 = &graphicsPassLatch,
				.DependencyLevelSpanIndex = (uint32_t)dependencyLevelSpanIndex,
			};

			auto executePassJob = [](void* userData, [[maybe_unused]] uint32_t userDataSize)
			{
				JobData* threadJobData = reinterpret_cast<JobData*>(userData);
				const FrameGraph* that = threadJobData->ExecuteParams->FrameGraph;

				ID3D12GraphicsCommandList6* graphicsCommandList      = that->mCommandListsRef->GetThreadDirectCommandList(threadJobData->DependencyLevelSpanIndex);
				ID3D12CommandAllocator*     graphicsCommandAllocator = that->mCommandListsRef->GetThreadDirectCommandAllocator(threadJobData->DependencyLevelSpanIndex, threadJobData->ExecuteParams->FrameResourceIndex);

				that->BeginCommandList(graphicsCommandList, graphicsCommandAllocator, threadJobData->DependencyLevelSpanIndex);
				
				std::array descriptorHeaps = {that->mDescriptorManagerRef->GetDescriptorHeap()};
				graphicsCommandList->SetDescriptorHeaps((UINT)descriptorHeaps.size(), descriptorHeaps.data());

				that->RecordGraphicsPasses(graphicsCommandList, threadJobData->ExecuteParams->Scene, threadJobData->DependencyLevelSpanIndex, threadJobData->ExecuteParams->FrameIndex, threadJobData->ExecuteParams->SwapchainImageIndex);
				that->EndCommandList(graphicsCommandList);

				threadJobData->Waitable->count_down();
			};

			threadPool->EnqueueWork(executePassJob, &jobData, sizeof(JobData));
		}

		ID3D12GraphicsCommandList6* mainGraphicsCommandList      = mCommandListsRef->GetMainThreadDirectCommandList();
		ID3D12CommandAllocator*     mainGraphicsCommandAllocator = mCommandListsRef->GetMainThreadDirectCommandAllocator(currentFrameResourceIndex);
		
		BeginCommandList(mainGraphicsCommandList, mainGraphicsCommandAllocator, (uint32_t)(mGraphicsPassSpansPerDependencyLevel.size() - 1));

		std::array descriptorHeaps = {mDescriptorManagerRef->GetDescriptorHeap()};
		mainGraphicsCommandList->SetDescriptorHeaps((UINT)descriptorHeaps.size(), descriptorHeaps.data());

		RecordGraphicsPasses(mainGraphicsCommandList, scene, (uint32_t)(mGraphicsPassSpansPerDependencyLevel.size() - 1), frameIndex, swapchainImageIndex);
		EndCommandList(mainGraphicsCommandList);

		graphicsPassLatch.wait();

		for(size_t i = 0; i < mGraphicsPassSpansPerDependencyLevel.size() - 1; i++)
		{
			mFrameRecordedGraphicsCommandLists[i] = mCommandListsRef->GetThreadDirectCommandList((uint32_t)i); //The command buffer that was used to record the command
		}

		mFrameRecordedGraphicsCommandLists[mGraphicsPassSpansPerDependencyLevel.size() - 1] = mainGraphicsCommandList;


		mDeviceQueuesRef->GraphicsQueueWaitForCopyQueue(scene->GetUploadFenceValue(currentFrameResourceIndex));

		ID3D12CommandQueue* graphicsQueue = mDeviceQueuesRef->GraphicsQueueHandle();
		graphicsQueue->ExecuteCommandLists((UINT)mGraphicsPassSpansPerDependencyLevel.size(), mFrameRecordedGraphicsCommandLists.data());
	}
}

void D3D12::FrameGraph::BeginCommandList(ID3D12GraphicsCommandList* commandList, ID3D12CommandAllocator* commandAllocator, [[maybe_unused]] uint32_t dependencyLevelSpanIndex) const
{
	//TODO: Reset with the first pipeline statre in the render pass
	THROW_IF_FAILED(commandAllocator->Reset());
	THROW_IF_FAILED(commandList->Reset(commandAllocator, nullptr));
}

void D3D12::FrameGraph::EndCommandList(ID3D12GraphicsCommandList* commandList) const
{
	THROW_IF_FAILED(commandList->Close());
}

void D3D12::FrameGraph::RecordGraphicsPasses(ID3D12GraphicsCommandList6* commandList, const RenderableScene* scene, uint32_t dependencyLevelSpanIndex, uint32_t frameIndex, uint32_t swapchainImageIndex) const
{
	Span<uint32_t> levelSpan = mGraphicsPassSpansPerDependencyLevel[dependencyLevelSpanIndex];
	for(uint32_t passSpanIndex = levelSpan.Begin; passSpanIndex < levelSpan.End; passSpanIndex++)
	{
		uint32_t passIndex = CalcPassIndex(mFrameSpansPerRenderPass[passSpanIndex], frameIndex, swapchainImageIndex);

		const BarrierPassSpan& barrierSpan            = mRenderPassBarriers[passIndex];
		UINT                   beforePassBarrierCount = barrierSpan.BeforePassEnd - barrierSpan.BeforePassBegin;
		UINT                   afterPassBarrierCount  = barrierSpan.AfterPassEnd  - barrierSpan.AfterPassBegin;

		if(beforePassBarrierCount != 0)
		{
			const D3D12_RESOURCE_BARRIER* barrierPointer = mResourceBarriers.data() + barrierSpan.BeforePassBegin;
			commandList->ResourceBarrier(beforePassBarrierCount, barrierPointer);
		}

		mRenderPasses[passIndex]->RecordExecution(commandList, scene, mFrameGraphConfig);

		if(afterPassBarrierCount != 0)
		{
			const D3D12_RESOURCE_BARRIER* barrierPointer = mResourceBarriers.data() + barrierSpan.AfterPassBegin;
			commandList->ResourceBarrier(afterPassBarrierCount, barrierPointer);
		}
	}
}