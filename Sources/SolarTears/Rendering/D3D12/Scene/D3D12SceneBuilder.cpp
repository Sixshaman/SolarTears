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

D3D12::RenderableSceneBuilder::RenderableSceneBuilder(ID3D12Device8* device, RenderableScene* sceneToBuild): ModernRenderableSceneBuilder(sceneToBuild), mDeviceRef(device), mD3d12SceneToBuild(sceneToBuild)
{
	mVertexBufferDesc   = {};
	mIndexBufferDesc    = {};
	mConstantBufferDesc = {};

	mSceneTextureDescs.clear();
}

D3D12::RenderableSceneBuilder::~RenderableSceneBuilder()
{
}

void D3D12::RenderableSceneBuilder::CreateVertexBufferPrerequisiteObject(size_t vertexDataSize)
{
	mVertexBufferDesc.Dimension                       = D3D12_RESOURCE_DIMENSION_BUFFER;
	mVertexBufferDesc.Alignment                       = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	mVertexBufferDesc.Width                           = vertexDataSize;
	mVertexBufferDesc.Height                          = 1;
	mVertexBufferDesc.DepthOrArraySize                = 1;
	mVertexBufferDesc.MipLevels                       = 1;
	mVertexBufferDesc.Format                          = DXGI_FORMAT_UNKNOWN;
	mVertexBufferDesc.SampleDesc.Count                = 1;
	mVertexBufferDesc.SampleDesc.Quality              = 0;
	mVertexBufferDesc.Layout                          = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	mVertexBufferDesc.Flags                           = D3D12_RESOURCE_FLAG_NONE;
	mVertexBufferDesc.SamplerFeedbackMipRegion.Width  = 0;
	mVertexBufferDesc.SamplerFeedbackMipRegion.Height = 0;
	mVertexBufferDesc.SamplerFeedbackMipRegion.Depth  = 0;
}

void D3D12::RenderableSceneBuilder::CreateIndexBufferPrerequisiteObject(size_t indexDataSize)
{
	mIndexBufferDesc.Dimension                       = D3D12_RESOURCE_DIMENSION_BUFFER;
	mIndexBufferDesc.Alignment                       = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	mIndexBufferDesc.Width                           = indexDataSize;
	mIndexBufferDesc.Height                          = 1;
	mIndexBufferDesc.DepthOrArraySize                = 1;
	mIndexBufferDesc.MipLevels                       = 1;
	mIndexBufferDesc.Format                          = DXGI_FORMAT_UNKNOWN;
	mIndexBufferDesc.SampleDesc.Count                = 1;
	mIndexBufferDesc.SampleDesc.Quality              = 0;
	mIndexBufferDesc.Layout                          = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	mIndexBufferDesc.Flags                           = D3D12_RESOURCE_FLAG_NONE;
	mIndexBufferDesc.SamplerFeedbackMipRegion.Width  = 1;
	mIndexBufferDesc.SamplerFeedbackMipRegion.Height = 1;
	mIndexBufferDesc.SamplerFeedbackMipRegion.Depth  = 1;
}

void D3D12::RenderableSceneBuilder::CreateConstantBufferPrerequisiteObject(size_t constantDataSize)
{
	mConstantBufferDesc.Dimension                       = D3D12_RESOURCE_DIMENSION_BUFFER;
	mConstantBufferDesc.Alignment                       = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	mConstantBufferDesc.Width                           = constantDataSize;
	mConstantBufferDesc.Height                          = 1;
	mConstantBufferDesc.DepthOrArraySize                = 1;
	mConstantBufferDesc.MipLevels                       = 1;
	mConstantBufferDesc.Format                          = DXGI_FORMAT_UNKNOWN;
	mConstantBufferDesc.SampleDesc.Count                = 1;
	mConstantBufferDesc.SampleDesc.Quality              = 0;
	mConstantBufferDesc.Layout                          = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	mConstantBufferDesc.Flags                           = D3D12_RESOURCE_FLAG_NONE;
	mConstantBufferDesc.SamplerFeedbackMipRegion.Width  = 1;
	mConstantBufferDesc.SamplerFeedbackMipRegion.Height = 1;
	mConstantBufferDesc.SamplerFeedbackMipRegion.Depth  = 1;
}

UINT64 D3D12::RenderableSceneBuilder::LoadTextureImages(ID3D12Device8* device, const std::vector<std::wstring>& sceneTextures, UINT64 currentIntermediateBufferSize)
{
	mIntermediateBufferTextureDataOffset = Utils::AlignMemory(currentIntermediateBufferSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
	UINT64 intermediateBufferSize        = mIntermediateBufferTextureDataOffset;

	mTextureData.clear();
	mSceneTextureDescs.clear();

	for (size_t i = 0; i < sceneTextures.size(); i++)
	{
		std::unique_ptr<uint8_t[]>          textureData;
		std::vector<D3D12_SUBRESOURCE_DATA> subresources;

		wil::com_ptr_nothrow<ID3D12Resource> texCommited = nullptr;
		THROW_IF_FAILED(DirectX::LoadDDSTextureFromFile(device, sceneTextures[i].c_str(), texCommited.put(), textureData, subresources));

		D3D12_RESOURCE_DESC texDesc = texCommited->GetDesc();

		D3D12_RESOURCE_DESC1 texDesc1;
		texDesc1.Dimension                       = texDesc.Dimension;
		texDesc1.Alignment                       = texDesc.Alignment;
		texDesc1.Width                           = texDesc.Width;
		texDesc1.Height                          = texDesc.Height;
		texDesc1.DepthOrArraySize                = texDesc.DepthOrArraySize;
		texDesc1.MipLevels                       = texDesc.MipLevels;
		texDesc1.Format                          = texDesc.Format;
		texDesc1.SampleDesc                      = texDesc.SampleDesc;
		texDesc1.Layout                          = texDesc.Layout;
		texDesc1.Flags                           = texDesc.Flags;
		texDesc1.SamplerFeedbackMipRegion.Width  = 0;
		texDesc1.SamplerFeedbackMipRegion.Height = 0;
		texDesc1.SamplerFeedbackMipRegion.Depth  = 0;

		mSceneTextureDescs.push_back(texDesc1);

		texCommited.reset();


		UINT64 textureByteSize = 0;
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(subresources.size());
		std::vector<UINT>                               footprintHeights(subresources.size());
		std::vector<UINT64>                             footprintByteWidths(subresources.size());
		device->GetCopyableFootprints1(&texDesc1, 0, (UINT)subresources.size(), intermediateBufferSize, footprints.data(), footprintHeights.data(), footprintByteWidths.data(), &textureByteSize);

		intermediateBufferSize += textureByteSize;

		size_t currentTextureOffset = mTextureData.size();
		mTextureData.resize(mTextureData.size() + textureByteSize);
		for(size_t j = 0; j < subresources.size(); j++)
		{
			//Mips are 256 byte aligned on a per-row basis
			if(footprintByteWidths[j] != footprints[j].Footprint.RowPitch)
			{
				size_t rowOffset = 0;
				for(size_t y = 0; y < footprintHeights[j]; y++)
				{
					memcpy(mTextureData.data() + currentTextureOffset + rowOffset, (uint8_t*)subresources[j].pData + y * subresources[j].RowPitch, subresources[j].RowPitch);
					rowOffset += footprints[j].Footprint.RowPitch;
				}

				currentTextureOffset += rowOffset;
			}
			else
			{
				memcpy(mTextureData.data() + currentTextureOffset, (uint8_t*)subresources[j].pData, subresources[j].SlicePitch * texDesc.DepthOrArraySize);
				currentTextureOffset += subresources[j].SlicePitch * texDesc.DepthOrArraySize;
			}
		}

		mSceneTextureSubresourceFootprints.push_back(footprints);
	}

	return intermediateBufferSize;
}

void D3D12::RenderableSceneBuilder::AllocateBuffersHeap(const MemoryManager* memoryAllocator)
{
	mSceneToBuild->mHeapForGpuBuffers.reset();
	mSceneToBuild->mHeapForCpuVisibleBuffers.reset();

	std::vector gpuBufferDescs          = {mVertexBufferDesc,        mIndexBufferDesc};
	std::vector gpuBufferOffsetPointers = {&mVertexBufferHeapOffset, &mIndexBufferHeapOffset};
	
	assert(gpuBufferDescs.size() == gpuBufferOffsetPointers.size()); //Just in case I add a new one and forget to add the other one

	std::vector<UINT64> gpuBufferOffsets;
	memoryAllocator->AllocateBuffersMemory(device, gpuBufferDescs, MemoryManager::BufferAllocationType::GPU_LOCAL, gpuBufferOffsets, mSceneToBuild->mHeapForGpuBuffers.put());

	for(size_t i = 0; i < gpuBufferOffsets.size(); i++)
	{
		*gpuBufferOffsetPointers[i] = gpuBufferOffsets[i];
	}


	std::vector cpuVisibleBufferDescs          = {mConstantBufferDesc};
	std::vector cpuVisibleBufferOffsetPointers = {&mConstantBufferHeapOffset};

	std::vector<UINT64> cpuVisibleBufferOffsets;
	memoryAllocator->AllocateBuffersMemory(device, cpuVisibleBufferDescs, MemoryManager::BufferAllocationType::CPU_VISIBLE, cpuVisibleBufferOffsets, mSceneToBuild->mHeapForCpuVisibleBuffers.put());

	for(size_t i = 0; i < cpuVisibleBufferOffsets.size(); i++)
	{
		*cpuVisibleBufferOffsetPointers[i] = cpuVisibleBufferOffsets[i];
	}
}

void D3D12::RenderableSceneBuilder::AllocateTexturesHeap(const MemoryManager* memoryAllocator)
{
	mSceneTextureHeapOffsets.clear();
	mSceneToBuild->mHeapForTextures.reset();

	memoryAllocator->AllocateTextureMemory(mDeviceRef, mSceneTextureDescs, mSceneTextureHeapOffsets, D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES, mSceneToBuild->mHeapForTextures.put());
}

void D3D12::RenderableSceneBuilder::CreateTextures()
{
	assert(mSceneTextureDescs.size() == mSceneTextureHeapOffsets.size());

	mSceneToBuild->mSceneTextures.reserve(mSceneTextureDescs.size());
	for(size_t i = 0; i < mSceneTextureDescs.size(); i++)
	{
		mSceneToBuild->mSceneTextures.emplace_back();
		THROW_IF_FAILED(device->CreatePlacedResource1(mSceneToBuild->mHeapForTextures.get(), mSceneTextureHeapOffsets[i], &mSceneTextureDescs[i], D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(mSceneToBuild->mSceneTextures.back().put())));
	}
}

void D3D12::RenderableSceneBuilder::CreateBuffers()
{
	std::array gpuBufferPointers    = {mSceneToBuild->mSceneVertexBuffer.put(), mSceneToBuild->mSceneIndexBuffer.put()};
	std::array gpuBufferDescs       = {mVertexBufferDesc,                       mIndexBufferDesc};
	std::array gpuBufferHeapOffsets = {mVertexBufferHeapOffset,                 mIndexBufferHeapOffset};
	std::array gpuBufferStates      = {D3D12_RESOURCE_STATE_COPY_DEST,          D3D12_RESOURCE_STATE_COPY_DEST};
	for (size_t i = 0; i < gpuBufferPointers.size(); i++)
	{
		THROW_IF_FAILED(device->CreatePlacedResource1(mSceneToBuild->mHeapForGpuBuffers.get(), gpuBufferHeapOffsets[i], &gpuBufferDescs[i], gpuBufferStates[i], nullptr, IID_PPV_ARGS(gpuBufferPointers[i])));
	}


	std::array cpuVisibleBufferPointers    = {mSceneToBuild->mSceneConstantBuffer.put()};
	std::array cpuVisibleBufferDescs       = {mConstantBufferDesc};
	std::array cpuVisibleBufferHeapOffsets = {mConstantBufferHeapOffset};
	std::array cpuVisibleBufferStates      = {D3D12_RESOURCE_STATE_GENERIC_READ};
	for (size_t i = 0; i < cpuVisibleBufferPointers.size(); i++)
	{
		THROW_IF_FAILED(device->CreatePlacedResource1(mSceneToBuild->mHeapForCpuVisibleBuffers.get(), cpuVisibleBufferHeapOffsets[i], &cpuVisibleBufferDescs[i], cpuVisibleBufferStates[i], nullptr, IID_PPV_ARGS(cpuVisibleBufferPointers[i])));
	}

	
	D3D12_RANGE readRange;
	readRange.Begin = 0;
	readRange.End   = 0;
	THROW_IF_FAILED(mSceneToBuild->mSceneConstantBuffer->Map(0, &readRange, &mSceneToBuild->mSceneConstantDataBufferPointer));
}

void D3D12::RenderableSceneBuilder::CreateBufferViews()
{
	mSceneToBuild->mSceneVertexBufferView.BufferLocation = mSceneToBuild->mSceneVertexBuffer->GetGPUVirtualAddress();
	mSceneToBuild->mSceneVertexBufferView.SizeInBytes    = (UINT)mVertexBufferDesc.Width;
	mSceneToBuild->mSceneVertexBufferView.StrideInBytes  = (UINT)sizeof(RenderableSceneVertex);

	mSceneToBuild->mSceneIndexBufferView.BufferLocation = mSceneToBuild->mSceneIndexBuffer->GetGPUVirtualAddress();
	mSceneToBuild->mSceneIndexBufferView.SizeInBytes    = (UINT)mIndexBufferDesc.Width;
	mSceneToBuild->mSceneIndexBufferView.Format         = D3D12Utils::FormatForIndexType<RenderableSceneIndex>;
}

void D3D12::RenderableSceneBuilder::CreateIntermediateBuffer(uint64_t intermediateBufferSize)
{
	D3D12_RESOURCE_DESC1 intermediateBufferDesc;
	intermediateBufferDesc.Dimension                       = D3D12_RESOURCE_DIMENSION_BUFFER;
	intermediateBufferDesc.Alignment                       = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	intermediateBufferDesc.Width                           = intermediateBufferSize;
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