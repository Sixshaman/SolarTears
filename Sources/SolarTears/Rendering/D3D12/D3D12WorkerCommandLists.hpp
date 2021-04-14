#pragma once

#include <d3d12.h>
#include <vector>
#include <wil/com.h>

namespace D3D12
{
	class DeviceQueues;

	class WorkerCommandLists
	{
	public:
		WorkerCommandLists(ID3D12Device8* device, uint32_t workerThreadCount, const DeviceQueues* deviceQueues);
		~WorkerCommandLists();

		uint32_t GetWorkerThreadCount() const;
	
		ID3D12CommandAllocator* GetMainThreadGraphicsCommandAllocator(uint32_t frameIndex) const;
		ID3D12CommandAllocator* GetMainThreadComputeCommandAllocator(uint32_t frameIndex)  const;
		ID3D12CommandAllocator* GetMainThreadCopyCommandAllocator(uint32_t frameIndex)     const;

		ID3D12CommandList* GetMainThreadGraphicsCommandList() const;
		ID3D12CommandList* GetMainThreadComputeCommandList()  const;
		ID3D12CommandList* GetMainThreadCopyCommandList()     const;

		ID3D12CommandAllocator* GetThreadGraphicsCommandAllocator(uint32_t threadIndex, uint32_t frameIndex) const;
		ID3D12CommandAllocator* GetThreadComputeCommandAllocator(uint32_t threadIndex, uint32_t frameIndex)  const;
		ID3D12CommandAllocator* GetThreadCopyCommandAllocator(uint32_t threadIndex, uint32_t frameIndex)     const;

		ID3D12CommandList* GetThreadGraphicsCommandList(uint32_t threadIndex) const;
		ID3D12CommandList* GetThreadComputeCommandList(uint32_t threadIndex)  const;
		ID3D12CommandList* GetThreadCopyCommandList(uint32_t threadIndex)     const;

	private:
		void InitCommandLists(ID3D12Device8* device);

	private:
		std::vector<wil::com_ptr_nothrow<ID3D12CommandAllocator>> mCommandAllocators;
		std::vector<wil::com_ptr_nothrow<ID3D12CommandList>>      mCommandLists;

		uint32_t mWorkerThreadCount;

		uint32_t mCommandListIndexForGraphics;
		uint32_t mCommandListIndexForCompute;
		uint32_t mCommandListIndexForCopy;

		const uint32_t mSeparateQueueCount = 3;
	};
}