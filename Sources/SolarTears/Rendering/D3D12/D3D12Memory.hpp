#pragma once

#include <d3d12.h>
#include "../../Logging/LoggerQueue.hpp"
#include "D3D12DeviceFeatures.hpp"
#include <vector>

namespace D3D12
{
	class MemoryManager
	{
	public:
		enum class BufferAllocationType
		{
			GPU_LOCAL,
			CPU_VISIBLE
		};

		MemoryManager(LoggerQueue* logger);
		~MemoryManager();

		void AllocateTextureMemory(ID3D12Device8* device, const std::vector<D3D12_RESOURCE_DESC1>& textureDescs, std::vector<UINT64>& outHeapOffsets, D3D12_HEAP_FLAGS heapFlags, ID3D12Heap** outHeap)         const;
		void AllocateBuffersMemory(ID3D12Device8* device, const std::vector<D3D12_RESOURCE_DESC1>& bufferDescs, BufferAllocationType allocationType, std::vector<UINT64>& outHeapOffsets, ID3D12Heap** outHeap) const;

	private:
		LoggerQueue* mLogger;
	};
}