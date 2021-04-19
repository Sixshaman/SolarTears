#include "D3D12Memory.hpp"
#include "D3D12Utils.hpp"
#include <cassert>
#include <wil/com.h>

D3D12::MemoryManager::MemoryManager(LoggerQueue* logger): mLogger(logger)
{
}

D3D12::MemoryManager::~MemoryManager()
{
}

void D3D12::MemoryManager::AllocateTextureMemory(ID3D12Device8* device, const std::vector<D3D12_RESOURCE_DESC1>& textureDescs, std::vector<UINT64>& outHeapOffsets, D3D12_HEAP_FLAGS heapFlags, ID3D12Heap** outHeap) const
{
	outHeapOffsets.clear();

	//TODO: mGPU?
	std::vector<D3D12_RESOURCE_ALLOCATION_INFO1> resourceAllocationInfos(textureDescs.size());
	D3D12_RESOURCE_ALLOCATION_INFO heapAllocInfo = device->GetResourceAllocationInfo2(0, (UINT)textureDescs.size(), textureDescs.data(), resourceAllocationInfos.data());

	for(D3D12_RESOURCE_ALLOCATION_INFO1 bufferAllocInfo: resourceAllocationInfos)
	{
		outHeapOffsets.push_back(bufferAllocInfo.Offset);
	}

	//TODO: mGPU?
	D3D12_HEAP_DESC heapDesc;
	heapDesc.SizeInBytes                     = heapAllocInfo.SizeInBytes;
	heapDesc.Alignment                       = heapAllocInfo.Alignment;
	heapDesc.Properties.Type                 = D3D12_HEAP_TYPE_DEFAULT;
	heapDesc.Properties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapDesc.Properties.CreationNodeMask     = 0;
	heapDesc.Properties.VisibleNodeMask      = 0;
	heapDesc.Flags                           = heapFlags;

	THROW_IF_FAILED(device->CreateHeap(&heapDesc, IID_PPV_ARGS(outHeap)));
}

void D3D12::MemoryManager::AllocateBuffersMemory(ID3D12Device8* device, const std::vector<D3D12_RESOURCE_DESC1>& bufferDescs, BufferAllocationType allocationType, std::vector<UINT64>& outHeapOffsets, ID3D12Heap** outHeap) const
{
	outHeapOffsets.clear();

	//TODO: mGPU?
	std::vector<D3D12_RESOURCE_ALLOCATION_INFO1> resourceAllocationInfos(bufferDescs.size());
	D3D12_RESOURCE_ALLOCATION_INFO heapAllocInfo = device->GetResourceAllocationInfo2(0, (UINT)bufferDescs.size(), bufferDescs.data(), resourceAllocationInfos.data());

	D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;
	switch (allocationType)
	{
	case BufferAllocationType::GPU_LOCAL:
		heapType = D3D12_HEAP_TYPE_DEFAULT;
		break;
	case BufferAllocationType::CPU_VISIBLE:
		heapType = D3D12_HEAP_TYPE_UPLOAD;
		break;
	default:
		break;
	}

	for(D3D12_RESOURCE_ALLOCATION_INFO1 bufferAllocInfo: resourceAllocationInfos)
	{
		outHeapOffsets.push_back(bufferAllocInfo.Offset);
	}

	//TODO: mGPU?
	D3D12_HEAP_DESC heapDesc;
	heapDesc.SizeInBytes                     = heapAllocInfo.SizeInBytes;
	heapDesc.Alignment                       = heapAllocInfo.Alignment;
	heapDesc.Properties.Type                 = heapType;
	heapDesc.Properties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapDesc.Properties.CreationNodeMask     = 0;
	heapDesc.Properties.VisibleNodeMask      = 0;
	heapDesc.Flags                           = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;

	THROW_IF_FAILED(device->CreateHeap(&heapDesc, IID_PPV_ARGS(outHeap)));
}