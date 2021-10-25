#include "D3D12SceneBuilder.hpp"
#include "../D3D12Utils.hpp"
#include "../D3D12Memory.hpp"
#include "../D3D12DeviceQueues.hpp"
#include "../D3D12WorkerCommandLists.hpp"
#include "../../Common/RenderingUtils.hpp"
#include "../../../../3rdParty/DirectXTex/DDSTextureLoader/DDSTextureLoader12.h"
#include <array>
#include <cassert>
#include <vector>

D3D12::RenderableSceneBuilder::RenderableSceneBuilder(ID3D12Device8* device, RenderableScene* sceneToBuild, 
	                                                  MemoryManager* memoryAllocator, DeviceQueues* deviceQueues, 
	                                                  const WorkerCommandLists* commandLists): ModernRenderableSceneBuilder(sceneToBuild, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT), mDeviceRef(device), mD3d12SceneToBuild(sceneToBuild),
	                                                                                           mMemoryAllocator(memoryAllocator), mDeviceQueues(deviceQueues), mWorkerCommandLists(commandLists)
{
	const D3D12_RESOURCE_DESC1 defaultBufferDesc = 
	{
		.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Alignment        = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT,
		.Width            = 0,
		.Height           = 1,
		.DepthOrArraySize = 1,
		.MipLevels        = 1,
		.Format           = DXGI_FORMAT_UNKNOWN,

		.SampleDesc = 
		{
			.Count   = 1,
			.Quality = 0,
		},

		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
		.Flags  = D3D12_RESOURCE_FLAG_NONE,

		.SamplerFeedbackMipRegion = 
		{
			.Width  = 0,
			.Height = 0,
			.Depth  = 0,
		}
	};

	mVertexBufferDesc          = defaultBufferDesc;
	mIndexBufferDesc           = defaultBufferDesc;
	mStaticConstantBufferDesc  = defaultBufferDesc;
	mDynamicConstantBufferDesc = defaultBufferDesc;
}

D3D12::RenderableSceneBuilder::~RenderableSceneBuilder()
{
}

void D3D12::RenderableSceneBuilder::CreateVertexBufferInfo(size_t vertexDataSize)
{
	mVertexBufferDesc.Width = vertexDataSize;
}

void D3D12::RenderableSceneBuilder::CreateIndexBufferInfo(size_t indexDataSize)
{
	mIndexBufferDesc.Width = indexDataSize;
}

void D3D12::RenderableSceneBuilder::CreateStaticConstantBufferInfo(size_t constantDataSize)
{
	mStaticConstantBufferDesc.Width = constantDataSize;
}

void D3D12::RenderableSceneBuilder::CreateDynamicConstantBufferInfo(size_t constantDataSize)
{
	mDynamicConstantBufferDesc.Width = constantDataSize;
}

void D3D12::RenderableSceneBuilder::AllocateTextureMetadataArrays(size_t textureCount)
{
	mSceneTextureDescs.resize(textureCount);
	mSceneTextureHeapOffsets.resize(textureCount);
	mSceneTextureSubresourceSlices.resize(textureCount);

	mSceneTextureSubresourceFootprints.clear();
}

void D3D12::RenderableSceneBuilder::LoadTextureFromFile(const std::wstring& textureFilename, uint64_t currentIntermediateBufferOffset, size_t textureIndex, std::vector<std::byte>& outTextureData)
{
	outTextureData.clear();

	std::unique_ptr<uint8_t[]>          textureData;
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;

	wil::com_ptr_nothrow<ID3D12Resource> texCommited = nullptr;
	THROW_IF_FAILED(DirectX::LoadDDSTextureFromFile(mDeviceRef, textureFilename.c_str(), texCommited.put(), textureData, subresources));

	D3D12_RESOURCE_DESC texDesc = texCommited->GetDesc();

	mSceneTextureDescs[textureIndex].Dimension                       = texDesc.Dimension;
	mSceneTextureDescs[textureIndex].Alignment                       = texDesc.Alignment;
	mSceneTextureDescs[textureIndex].Width                           = texDesc.Width;
	mSceneTextureDescs[textureIndex].Height                          = texDesc.Height;
	mSceneTextureDescs[textureIndex].DepthOrArraySize                = texDesc.DepthOrArraySize;
	mSceneTextureDescs[textureIndex].MipLevels                       = texDesc.MipLevels;
	mSceneTextureDescs[textureIndex].Format                          = texDesc.Format;
	mSceneTextureDescs[textureIndex].SampleDesc                      = texDesc.SampleDesc;
	mSceneTextureDescs[textureIndex].Layout                          = texDesc.Layout;
	mSceneTextureDescs[textureIndex].Flags                           = texDesc.Flags;
	mSceneTextureDescs[textureIndex].SamplerFeedbackMipRegion.Width  = 0;
	mSceneTextureDescs[textureIndex].SamplerFeedbackMipRegion.Height = 0;
	mSceneTextureDescs[textureIndex].SamplerFeedbackMipRegion.Depth  = 0;

	texCommited.reset();

	uint32_t subresourceSliceBegin = (uint32_t)(mSceneTextureSubresourceFootprints.size());
	uint32_t subresourceSliceEnd   = (uint32_t)(mSceneTextureSubresourceFootprints.size() + subresources.size());
	mSceneTextureSubresourceSlices[textureIndex] = {.Begin = subresourceSliceBegin, .End = subresourceSliceEnd};
	mSceneTextureSubresourceFootprints.resize(mSceneTextureSubresourceFootprints.size() + subresources.size());

	UINT64 textureByteSize = 0;
	std::vector<UINT>   footprintHeights(subresources.size());
	std::vector<UINT64> footprintByteWidths(subresources.size());
	mDeviceRef->GetCopyableFootprints1(&mSceneTextureDescs[textureIndex], 0, (UINT)subresources.size(), currentIntermediateBufferOffset, mSceneTextureSubresourceFootprints.data() + subresourceSliceBegin, footprintHeights.data(), footprintByteWidths.data(), &textureByteSize);

	outTextureData.resize(textureByteSize);

	size_t currentTextureOffset = 0;
	for(uint32_t j = 0; j < subresources.size(); j++)
	{
		uint32_t globalSubresourceIndex = subresourceSliceBegin + j;

		//Mips are 256 byte aligned on a per-row basis
		if(footprintByteWidths[j] != mSceneTextureSubresourceFootprints[globalSubresourceIndex].Footprint.RowPitch)
		{
			size_t rowOffset = 0;
			for(size_t y = 0; y < footprintHeights[j]; y++)
			{
				memcpy(outTextureData.data() + currentTextureOffset + rowOffset, (uint8_t*)subresources[j].pData + y * subresources[j].RowPitch, subresources[j].RowPitch);
				rowOffset += mSceneTextureSubresourceFootprints[globalSubresourceIndex].Footprint.RowPitch;
			}

			currentTextureOffset += rowOffset;
		}
		else
		{
			memcpy(outTextureData.data() + currentTextureOffset, (uint8_t*)subresources[j].pData, subresources[j].SlicePitch * texDesc.DepthOrArraySize);
			currentTextureOffset += subresources[j].SlicePitch * texDesc.DepthOrArraySize;
		}
	}
}

void D3D12::RenderableSceneBuilder::FinishBufferCreation()
{
	AllocateBuffersHeap();
	CreateBufferObjects();
	CreateBufferViews();
}

void D3D12::RenderableSceneBuilder::FinishTextureCreation()
{
	AllocateTexturesHeap();
	CreateTextureObjects();
}

std::byte* D3D12::RenderableSceneBuilder::MapDynamicConstantBuffer()
{
	D3D12_RANGE readRange;
	readRange.Begin = 0;
	readRange.End   = 0;

	std::byte* bufferPointer = nullptr;
	THROW_IF_FAILED(mD3d12SceneToBuild->mSceneDynamicConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&bufferPointer)));

	return bufferPointer;
}

void D3D12::RenderableSceneBuilder::CreateIntermediateBuffer()
{
	D3D12_RESOURCE_DESC1 intermediateBufferDesc;
	intermediateBufferDesc.Dimension                       = D3D12_RESOURCE_DIMENSION_BUFFER;
	intermediateBufferDesc.Alignment                       = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	intermediateBufferDesc.Width                           = mIntermediateBufferSize;
	intermediateBufferDesc.Height                          = 1;
	intermediateBufferDesc.DepthOrArraySize                = 1;
	intermediateBufferDesc.MipLevels                       = 1;
	intermediateBufferDesc.Format                          = DXGI_FORMAT_UNKNOWN;
	intermediateBufferDesc.SampleDesc.Count                = 1;
	intermediateBufferDesc.SampleDesc.Quality              = 0;
	intermediateBufferDesc.Layout                          = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	intermediateBufferDesc.Flags                           = D3D12_RESOURCE_FLAG_NONE;
	intermediateBufferDesc.SamplerFeedbackMipRegion.Width  = 1;
	intermediateBufferDesc.SamplerFeedbackMipRegion.Height = 1;
	intermediateBufferDesc.SamplerFeedbackMipRegion.Depth  = 1;

	//TODO: mGPU?
	D3D12_HEAP_PROPERTIES intermediateBufferHeapProperties;
	intermediateBufferHeapProperties.Type                 = D3D12_HEAP_TYPE_UPLOAD;
	intermediateBufferHeapProperties.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	intermediateBufferHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	intermediateBufferHeapProperties.CreationNodeMask     = 0;
	intermediateBufferHeapProperties.VisibleNodeMask      = 0;

	THROW_IF_FAILED(mDeviceRef->CreateCommittedResource2(&intermediateBufferHeapProperties, D3D12_HEAP_FLAG_NONE, &intermediateBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, nullptr, IID_PPV_ARGS(mIntermediateBuffer.put())));
}

inline std::byte* D3D12::RenderableSceneBuilder::MapIntermediateBuffer() const
{
	D3D12_RANGE readRange;
	readRange.Begin = 0;
	readRange.End   = 0;

	void* bufferData = nullptr;
	THROW_IF_FAILED(mIntermediateBuffer->Map(0, &readRange, &bufferData));

	return reinterpret_cast<std::byte*>(bufferData);
}

inline void D3D12::RenderableSceneBuilder::UnmapIntermediateBuffer() const
{
	mIntermediateBuffer->Unmap(0, nullptr);
}

void D3D12::RenderableSceneBuilder::WriteInitializationCommands() const
{
	ID3D12CommandAllocator*    commandAllocator = mWorkerCommandLists->GetMainThreadDirectCommandAllocator(0);
	ID3D12GraphicsCommandList* commandList      = mWorkerCommandLists->GetMainThreadDirectCommandList();

	THROW_IF_FAILED(commandAllocator->Reset());
	THROW_IF_FAILED(commandList->Reset(commandAllocator, nullptr));

	for(size_t i = 0; i < mSceneTextureSubresourceSlices.size(); i++)
	{
		SubresourceArraySlice subresourceSlice = mSceneTextureSubresourceSlices[i];
		for(uint32_t j = subresourceSlice.Begin; j < subresourceSlice.End; j++)
		{
			D3D12_TEXTURE_COPY_LOCATION dstLocation;
			dstLocation.pResource        = mD3d12SceneToBuild->mSceneTextures[i].get();
			dstLocation.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dstLocation.SubresourceIndex = (UINT)(j - subresourceSlice.Begin);

			D3D12_TEXTURE_COPY_LOCATION srcLocation;
			srcLocation.pResource       = mIntermediateBuffer.get();
			srcLocation.Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			srcLocation.PlacedFootprint = mSceneTextureSubresourceFootprints[j];

			commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);
		}
	}


	std::array sceneBuffers           = {mD3d12SceneToBuild->mSceneVertexBuffer.get(),             mD3d12SceneToBuild->mSceneIndexBuffer.get(),            mD3d12SceneToBuild->mSceneStaticConstantBuffer.get()};
	std::array sceneBufferDataOffsets = {mIntermediateBufferVertexDataOffset,                      mIntermediateBufferIndexDataOffset,                     mIntermediateBufferStaticConstantDataOffset};
	std::array sceneBufferDataSizes   = {mVertexBufferData.size() * sizeof(RenderableSceneVertex), mIndexBufferData.size() * sizeof(RenderableSceneIndex), mStaticConstantData.size() * sizeof(std::byte)};
	std::array sceneBufferStates      = {D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,          D3D12_RESOURCE_STATE_INDEX_BUFFER,                      D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER};
	for(size_t i = 0; i < sceneBuffers.size(); i++)
	{
		commandList->CopyBufferRegion(sceneBuffers[i], 0, mIntermediateBuffer.get(), sceneBufferDataOffsets[i], sceneBufferDataSizes[i]);
	}


	std::vector<D3D12_RESOURCE_BARRIER> barriers;
	barriers.reserve(mD3d12SceneToBuild->mSceneTextures.size() + sceneBuffers.size());
	for(int i = 0; i < mD3d12SceneToBuild->mSceneTextures.size(); i++)
	{
		D3D12_RESOURCE_BARRIER textureBarrier;
		textureBarrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		textureBarrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		textureBarrier.Transition.pResource   = mD3d12SceneToBuild->mSceneTextures[i].get();
		textureBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		textureBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		textureBarrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		barriers.push_back(textureBarrier);
	}

	for(int i = 0; i < sceneBuffers.size(); i++)
	{
		D3D12_RESOURCE_BARRIER bufferBarrier;
		bufferBarrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		bufferBarrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		bufferBarrier.Transition.pResource   = sceneBuffers[i];
		bufferBarrier.Transition.Subresource = 0;
		bufferBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		bufferBarrier.Transition.StateAfter  = sceneBufferStates[i];

		barriers.push_back(bufferBarrier);
	}

	commandList->ResourceBarrier((UINT)barriers.size(), barriers.data());


	THROW_IF_FAILED(commandList->Close());
}

void D3D12::RenderableSceneBuilder::SubmitInitializationCommands() const
{
	ID3D12CommandQueue* graphicsQueue = mDeviceQueues->GraphicsQueueHandle();

	std::array commandListsToExecute = {reinterpret_cast<ID3D12CommandList*>(mWorkerCommandLists->GetMainThreadDirectCommandList())};
	graphicsQueue->ExecuteCommandLists((UINT)commandListsToExecute.size(), commandListsToExecute.data());
}

void D3D12::RenderableSceneBuilder::WaitForInitializationCommands() const
{
	UINT64 waitFenceValue = mDeviceQueues->GraphicsFenceSignal();
	mDeviceQueues->GraphicsQueueCpuWait(waitFenceValue);
}

void D3D12::RenderableSceneBuilder::AllocateBuffersHeap()
{
	mD3d12SceneToBuild->mHeapForGpuBuffers.reset();
	mD3d12SceneToBuild->mHeapForCpuVisibleBuffers.reset();

	std::vector gpuBufferDescs          = {mVertexBufferDesc,             mIndexBufferDesc,             mStaticConstantBufferDesc};
	std::vector gpuBufferOffsetPointers = {&mVertexBufferGpuMemoryOffset, &mIndexBufferGpuMemoryOffset, &mStaticConstantBufferGpuMemoryOffset};
	
	assert(gpuBufferDescs.size() == gpuBufferOffsetPointers.size()); //Just in case I add a new one and forget to add the other one

	std::vector<UINT64> gpuBufferOffsets;
	mMemoryAllocator->AllocateBuffersMemory(mDeviceRef, gpuBufferDescs, MemoryManager::BufferAllocationType::GPU_LOCAL, gpuBufferOffsets, mD3d12SceneToBuild->mHeapForGpuBuffers.put());

	for(size_t i = 0; i < gpuBufferOffsets.size(); i++)
	{
		*gpuBufferOffsetPointers[i] = gpuBufferOffsets[i];
	}


	std::vector cpuVisibleBufferDescs          = {mDynamicConstantBufferDesc};
	std::vector cpuVisibleBufferOffsetPointers = {&mDynamicConstantBufferGpuMemoryOffset};

	std::vector<UINT64> cpuVisibleBufferOffsets;
	mMemoryAllocator->AllocateBuffersMemory(mDeviceRef, cpuVisibleBufferDescs, MemoryManager::BufferAllocationType::CPU_VISIBLE, cpuVisibleBufferOffsets, mD3d12SceneToBuild->mHeapForCpuVisibleBuffers.put());

	for(size_t i = 0; i < cpuVisibleBufferOffsets.size(); i++)
	{
		*cpuVisibleBufferOffsetPointers[i] = cpuVisibleBufferOffsets[i];
	}
}

void D3D12::RenderableSceneBuilder::AllocateTexturesHeap()
{
	mSceneTextureHeapOffsets.clear();
	mD3d12SceneToBuild->mHeapForTextures.reset();

	mMemoryAllocator->AllocateTextureMemory(mDeviceRef, mSceneTextureDescs, mSceneTextureHeapOffsets, D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES, mD3d12SceneToBuild->mHeapForTextures.put());
}

void D3D12::RenderableSceneBuilder::CreateBufferObjects()
{
	std::array gpuBufferPointers    = {mD3d12SceneToBuild->mSceneVertexBuffer.put(), mD3d12SceneToBuild->mSceneIndexBuffer.put(), mD3d12SceneToBuild->mSceneStaticConstantBuffer.put()};
	std::array gpuBufferDescs       = {mVertexBufferDesc,                            mIndexBufferDesc,                            mStaticConstantBufferDesc};
	std::array gpuBufferHeapOffsets = {mVertexBufferGpuMemoryOffset,                 mIndexBufferGpuMemoryOffset,                 mStaticConstantBufferGpuMemoryOffset};
	std::array gpuBufferStates      = {D3D12_RESOURCE_STATE_COPY_DEST,               D3D12_RESOURCE_STATE_COPY_DEST,              D3D12_RESOURCE_STATE_COPY_DEST};
	for (size_t i = 0; i < gpuBufferPointers.size(); i++)
	{
		THROW_IF_FAILED(mDeviceRef->CreatePlacedResource1(mD3d12SceneToBuild->mHeapForGpuBuffers.get(), gpuBufferHeapOffsets[i], &gpuBufferDescs[i], gpuBufferStates[i], nullptr, IID_PPV_ARGS(gpuBufferPointers[i])));
	}


	std::array cpuVisibleBufferPointers    = {mD3d12SceneToBuild->mSceneDynamicConstantBuffer.put()};
	std::array cpuVisibleBufferDescs       = {mDynamicConstantBufferDesc};
	std::array cpuVisibleBufferHeapOffsets = {mDynamicConstantBufferGpuMemoryOffset};
	std::array cpuVisibleBufferStates      = {D3D12_RESOURCE_STATE_GENERIC_READ};
	for (size_t i = 0; i < cpuVisibleBufferPointers.size(); i++)
	{
		THROW_IF_FAILED(mDeviceRef->CreatePlacedResource1(mD3d12SceneToBuild->mHeapForCpuVisibleBuffers.get(), cpuVisibleBufferHeapOffsets[i], &cpuVisibleBufferDescs[i], cpuVisibleBufferStates[i], nullptr, IID_PPV_ARGS(cpuVisibleBufferPointers[i])));
	}
}

void D3D12::RenderableSceneBuilder::CreateTextureObjects()
{
	assert(mSceneTextureDescs.size() == mSceneTextureHeapOffsets.size());

	mD3d12SceneToBuild->mSceneTextures.reserve(mSceneTextureDescs.size());
	for(size_t i = 0; i < mSceneTextureDescs.size(); i++)
	{
		mD3d12SceneToBuild->mSceneTextures.emplace_back();
		THROW_IF_FAILED(mDeviceRef->CreatePlacedResource1(mD3d12SceneToBuild->mHeapForTextures.get(), mSceneTextureHeapOffsets[i], &mSceneTextureDescs[i], D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(mD3d12SceneToBuild->mSceneTextures.back().put())));
	}
}

void D3D12::RenderableSceneBuilder::CreateBufferViews()
{
	mD3d12SceneToBuild->mSceneVertexBufferView.BufferLocation = mD3d12SceneToBuild->mSceneVertexBuffer->GetGPUVirtualAddress();
	mD3d12SceneToBuild->mSceneVertexBufferView.SizeInBytes    = (UINT)mVertexBufferDesc.Width;
	mD3d12SceneToBuild->mSceneVertexBufferView.StrideInBytes  = (UINT)sizeof(RenderableSceneVertex);

	mD3d12SceneToBuild->mSceneIndexBufferView.BufferLocation = mD3d12SceneToBuild->mSceneIndexBuffer->GetGPUVirtualAddress();
	mD3d12SceneToBuild->mSceneIndexBufferView.SizeInBytes    = (UINT)mIndexBufferDesc.Width;
	mD3d12SceneToBuild->mSceneIndexBufferView.Format         = D3D12Utils::FormatForIndexType<RenderableSceneIndex>;
}