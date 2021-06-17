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

D3D12::FrameGraph::FrameGraph(const FrameGraphConfig& frameGraphConfig): ModernFrameGraph(frameGraphConfig)
{
	mSrvUavDescriptorCount = 0;
}

D3D12::FrameGraph::~FrameGraph()
{
}

void D3D12::FrameGraph::Traverse(ThreadPool* threadPool, const WorkerCommandLists* commandLists, const RenderableScene* scene, const ShaderManager* shaderManager, const SrvDescriptorManager* descriptorManager, DeviceQueues* deviceQueues, uint32_t frameIndex, uint32_t swapchainImageIndex)
{
	SwitchBarrierTextures(swapchainImageIndex, frameIndex);

	uint32_t currentFrameResourceIndex = frameIndex % Utils::InFlightFrameCount;
	if(mGraphicsPassSpans.size() > 0)
	{
		struct ExecuteParameters
		{
			const WorkerCommandLists*   CommandLists;
			const FrameGraph*           FrameGraph;
			const RenderableScene*      Scene;
			const ShaderManager*        Shaders;
			const SrvDescriptorManager* DescriptorManager;

			uint32_t FrameIndex;
			uint32_t FrameResourceIndex;
			uint32_t SwapchainImageIndex;
		} 
		executeParameters = 
		{
			.CommandLists      = commandLists,
			.FrameGraph        = this,
			.Scene             = scene,
			.Shaders           = shaderManager,
			.DescriptorManager = descriptorManager,

			.FrameIndex          = frameIndex,
			.FrameResourceIndex  = currentFrameResourceIndex,
			.SwapchainImageIndex = swapchainImageIndex
		};

		std::latch graphicsPassLatch((uint32_t)(mGraphicsPassSpans.size() - 1));
		for(size_t dependencyLevelSpanIndex = 0; dependencyLevelSpanIndex < mGraphicsPassSpans.size() - 1; dependencyLevelSpanIndex++)
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

			auto executePassJob = [](void* userData, uint32_t userDataSize)
			{
				UNREFERENCED_PARAMETER(userDataSize);

				JobData* threadJobData = reinterpret_cast<JobData*>(userData);
				const FrameGraph* that = threadJobData->ExecuteParams->FrameGraph;

				ID3D12GraphicsCommandList6* graphicsCommandList      = threadJobData->ExecuteParams->CommandLists->GetThreadDirectCommandList(threadJobData->DependencyLevelSpanIndex);
				ID3D12CommandAllocator*     graphicsCommandAllocator = threadJobData->ExecuteParams->CommandLists->GetThreadDirectCommandAllocator(threadJobData->DependencyLevelSpanIndex, threadJobData->ExecuteParams->FrameResourceIndex);

				that->BeginCommandList(graphicsCommandList, graphicsCommandAllocator, threadJobData->DependencyLevelSpanIndex);
				
				std::array descriptorHeaps = { threadJobData->ExecuteParams->DescriptorManager->GetSrvUavCbvHeap()};
				graphicsCommandList->SetDescriptorHeaps((UINT)descriptorHeaps.size(), descriptorHeaps.data());

				that->RecordGraphicsPasses(graphicsCommandList, threadJobData->ExecuteParams->Scene, threadJobData->ExecuteParams->Shaders, threadJobData->DependencyLevelSpanIndex, threadJobData->ExecuteParams->FrameIndex, threadJobData->ExecuteParams->SwapchainImageIndex);
				that->EndCommandList(graphicsCommandList);

				threadJobData->Waitable->count_down();
			};

			threadPool->EnqueueWork(executePassJob, &jobData, sizeof(JobData));
		}

		ID3D12GraphicsCommandList6* mainGraphicsCommandList      = commandLists->GetMainThreadDirectCommandList();
		ID3D12CommandAllocator*     mainGraphicsCommandAllocator = commandLists->GetMainThreadDirectCommandAllocator(currentFrameResourceIndex);
		
		BeginCommandList(mainGraphicsCommandList, mainGraphicsCommandAllocator, (uint32_t)(mGraphicsPassSpans.size() - 1));
		RecordGraphicsPasses(mainGraphicsCommandList, scene, shaderManager, (uint32_t)(mGraphicsPassSpans.size() - 1), frameIndex, swapchainImageIndex);
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
}

void D3D12::FrameGraph::BeginCommandList(ID3D12GraphicsCommandList* commandList, ID3D12CommandAllocator* commandAllocator, uint32_t dependencyLevelSpanIndex) const
{
	UNREFERENCED_PARAMETER(dependencyLevelSpanIndex);

	//TODO: Reset with the first pipeline statre in the render pass
	THROW_IF_FAILED(commandAllocator->Reset());
	THROW_IF_FAILED(commandList->Reset(commandAllocator, nullptr));
}

void D3D12::FrameGraph::EndCommandList(ID3D12GraphicsCommandList* commandList) const
{
	THROW_IF_FAILED(commandList->Close());
}

void D3D12::FrameGraph::RecordGraphicsPasses(ID3D12GraphicsCommandList6* commandList, const RenderableScene* scene, const ShaderManager* shaderManager, uint32_t dependencyLevelSpanIndex, uint32_t frameIndex, uint32_t swapchainImageIndex) const
{
	Span<uint32_t> levelSpan = mGraphicsPassSpans[dependencyLevelSpanIndex];
	for(uint32_t passSpanIndex = levelSpan.Begin; passSpanIndex < levelSpan.End; passSpanIndex++)
	{
		const RenderPassSpanInfo passSpanInfo = mPassFrameSpans[passSpanIndex];

		uint32_t renderPassFrame = passSpanInfo.OwnFrames * swapchainImageIndex + frameIndex % passSpanInfo.OwnFrames;
		uint32_t renderPassIndex = passSpanInfo.PassSpanBegin + renderPassFrame;

		BarrierSpan barrierSpan            = mRenderPassBarriers[passSpanIndex];
		UINT        beforePassBarrierCount = barrierSpan.BeforePassEnd - barrierSpan.BeforePassBegin;
		UINT        afterPassBarrierCount  = barrierSpan.AfterPassEnd  - barrierSpan.AfterPassBegin;

		if(beforePassBarrierCount != 0)
		{
			const D3D12_RESOURCE_BARRIER* barrierPointer = mResourceBarriers.data() + barrierSpan.BeforePassBegin;
			commandList->ResourceBarrier(beforePassBarrierCount, barrierPointer);
		}

		mRenderPasses[renderPassIndex]->RecordExecution(commandList, scene, shaderManager, mFrameGraphConfig);

		if(afterPassBarrierCount != 0)
		{
			const D3D12_RESOURCE_BARRIER* barrierPointer = mResourceBarriers.data() + barrierSpan.AfterPassBegin;
			commandList->ResourceBarrier(afterPassBarrierCount, barrierPointer);
		}
	}
}

void D3D12::FrameGraph::SwitchBarrierTextures(uint32_t swapchainImageIndex, uint32_t frameIndex)
{
	//Update barriers
	for(MultiframeBarrierInfo barrierInfo: mMultiframeBarrierInfos)
	{
		bool isSwapchainTex = barrierInfo.BaseTexIndex == mBackbufferImageSpan.Begin;
		uint32_t frame = (uint32_t)(isSwapchainTex) * swapchainImageIndex + (uint32_t)(!isSwapchainTex) * (frameIndex % barrierInfo.TexturePeriod);
		mResourceBarriers[barrierInfo.BarrierIndex].Transition.pResource = mTextures[barrierInfo.BaseTexIndex + frame];
	}
}