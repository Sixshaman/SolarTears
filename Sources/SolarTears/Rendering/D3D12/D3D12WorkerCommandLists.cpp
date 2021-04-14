#include "D3D12WorkerCommandLists.hpp"
#include "D3D12Utils.hpp"
#include "D3D12DeviceQueues.hpp"
#include <unordered_set>
#include <array>

D3D12::WorkerCommandLists::WorkerCommandLists(ID3D12Device8* device, uint32_t workerThreadCount, const DeviceQueues* deviceQueues): mWorkerThreadCount(workerThreadCount),
	                                                                                                                                mCommandListIndexForGraphics((uint32_t)(-1)), 
	                                                                                                                                mCommandListIndexForCompute((uint32_t)(-1)),
	                                                                                                                                mCommandListIndexForCopy((uint32_t)(-1))
{
	InitCommandLists(device);
}

D3D12::WorkerCommandLists::~WorkerCommandLists()
{
}

uint32_t D3D12::WorkerCommandLists::GetWorkerThreadCount() const
{
	return mWorkerThreadCount;
}

void D3D12::WorkerCommandLists::InitCommandLists(ID3D12Device8* device)
{
	std::array commandListTypes = {D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_COMPUTE, D3D12_COMMAND_LIST_TYPE_COPY};

	//1 for each worker thread plus 1 for the main thread per queue per frame
	for(uint32_t t = 0; t < mWorkerThreadCount + 1; t++)
	{
		for(D3D12_COMMAND_LIST_TYPE commandListType: commandListTypes)
		{
			for(uint32_t f = 0; f < D3D12::D3D12Utils::InFlightFrameCount; f++)
			{
				mCommandAllocators.emplace_back();
				THROW_IF_FAILED(device->CreateCommandAllocator(commandListType, IID_PPV_ARGS(mCommandAllocators.back().put())));
			}

			mCommandLists.emplace_back();
			THROW_IF_FAILED(device->CreateCommandList1(0, commandListType, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(mCommandLists.back().put())));
		}
	}

	mCommandListIndexForGraphics = 0;
	mCommandListIndexForCompute  = 1;
	mCommandListIndexForCopy     = 2;
}

ID3D12CommandAllocator* D3D12::WorkerCommandLists::GetMainThreadGraphicsCommandAllocator(uint32_t frameIndex) const
{
	//Main thread command buffers/pools are always at the end
	size_t alligatorIndex = (size_t)(mWorkerThreadCount + 1) * mSeparateQueueCount * (size_t)frameIndex  + mWorkerThreadCount * mSeparateQueueCount + mCommandListIndexForGraphics;
	return mCommandPools[poolIndex];
}

ID3D12CommandAllocator* D3D12::WorkerCommandLists::GetMainThreadComputeCommandAllocator(uint32_t frameIndex) const
{
	//Main thread command buffers/pools are always at the end
	size_t allocatorIndex = (size_t)(mWorkerThreadCount + 1) * (size_t)frameIndex * mSeparateQueueCount + mWorkerThreadCount * mSeparateQueueCount + mCommandListIndexForCompute;
	return mCommandPools[poolIndex];
}

ID3D12CommandAllocator* D3D12::WorkerCommandLists::GetMainThreadCopyCommandAllocator(uint32_t frameIndex) const
{
	//Main thread command buffers/pools are always at the end
	size_t allocatorIndex = (size_t)(mWorkerThreadCount + 1) * (size_t)frameIndex * mSeparateQueueCount + mWorkerThreadCount * mSeparateQueueCount + mCommandListIndexForCopy;
	return mCommandPools[poolIndex];
}

ID3D12CommandList* D3D12::WorkerCommandLists::GetMainThreadGraphicsCommandList() const
{
	//Main thread command buffers/pools are always at the end
	size_t listIndex = (size_t)(mWorkerThreadCount + 1) * (size_t)frameIndex * mSeparateQueueCount + mWorkerThreadCount * mSeparateQueueCount + mCommandListIndexForGraphics;
	return mCommandBuffers[bufferIndex];
}

ID3D12CommandList* D3D12::WorkerCommandLists::GetMainThreadComputeCommandList() const
{
	//Main thread command buffers/pools are always at the end
	size_t listIndex = (size_t)(mWorkerThreadCount + 1) * (size_t)frameIndex * mSeparateQueueCount + mWorkerThreadCount * mSeparateQueueCount + mCommandListIndexForCompute;
	return mCommandBuffers[bufferIndex];
}

ID3D12CommandList* D3D12::WorkerCommandLists::GetMainThreadCopyCommandList() const
{
	//Main thread command buffers/pools are always at the end
	size_t listIndex = (size_t)(mWorkerThreadCount + 1) * (size_t)frameIndex * mSeparateQueueCount + mWorkerThreadCount * mSeparateQueueCount + mCommandListIndexForCopy;
	return mCommandBuffers[bufferIndex];
}

ID3D12CommandAllocator* D3D12::WorkerCommandLists::GetThreadGraphicsCommandAllocator(uint32_t threadIndex, uint32_t frameIndex) const
{
	size_t allocatorIndex = (size_t)(mWorkerThreadCount + 1) * (size_t)frameIndex * mSeparateQueueCount + threadIndex * mSeparateQueueCount + mCommandListIndexForGraphics;
	return mCommandPools[poolIndex];
}

ID3D12CommandAllocator* D3D12::WorkerCommandLists::GetThreadComputeCommandAllocator(uint32_t threadIndex, uint32_t frameIndex) const
{
	size_t allocatorIndex = (size_t)(mWorkerThreadCount + 1) * (size_t)frameIndex * mSeparateQueueCount + threadIndex * mSeparateQueueCount + mCommandListIndexForCompute;
	return mCommandPools[poolIndex];
}

ID3D12CommandAllocator* D3D12::WorkerCommandLists::GetThreadCopyCommandAllocator(uint32_t threadIndex, uint32_t frameIndex) const
{
	size_t allocatorIndex = (size_t)(mWorkerThreadCount + 1) * (size_t)frameIndex * mSeparateQueueCount + threadIndex * mSeparateQueueCount + mCommandListIndexForCopy;
	return mCommandPools[poolIndex];
}

ID3D12CommandList* D3D12::WorkerCommandLists::GetThreadGraphicsCommandList(uint32_t threadIndex) const
{
	size_t listIndex = (size_t)(mWorkerThreadCount + 1) * (size_t)frameIndex * mSeparateQueueCount + threadIndex * mSeparateQueueCount + mCommandListIndexForGraphics;
	return mCommandBuffers[bufferIndex];
}

ID3D12CommandList* D3D12::WorkerCommandLists::GetThreadComputeCommandList(uint32_t threadIndex) const
{
	size_t listIndex = (size_t)(mWorkerThreadCount + 1) * (size_t)frameIndex * mSeparateQueueCount + threadIndex * mSeparateQueueCount + mCommandListIndexForCompute;
	return mCommandBuffers[bufferIndex];
}

ID3D12CommandList* D3D12::WorkerCommandLists::GetThreadCopyCommandList(uint32_t threadIndex) const
{
	size_t listIndex = (size_t)(mWorkerThreadCount + 1) * (size_t)frameIndex * mSeparateQueueCount + threadIndex * mSeparateQueueCount + mCommandListIndexForCopy;
	return mCommandBuffers[bufferIndex];
}