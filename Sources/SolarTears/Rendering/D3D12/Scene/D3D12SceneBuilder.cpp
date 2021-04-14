#include "D3D12SceneBuilder.hpp"
#include "../D3D12Utils.hpp"
#include "../D3D12Memory.hpp"
#include "../../../../3rd party/DirectXTex/DDSTextureLoader/DDSTextureLoader12.h"
#include <array>
#include <cassert>
#include <vector>

D3D12::RenderableSceneBuilder::RenderableSceneBuilder(RenderableScene* sceneToBuild): mSceneToBuild(sceneToBuild)
{
	assert(sceneToBuild != nullptr);
}

D3D12::RenderableSceneBuilder::~RenderableSceneBuilder()
{
}

void D3D12::RenderableSceneBuilder::BakeSceneFirstPart(ID3D12Device8* device, const DeviceQueues* deviceQueues, const MemoryManager* memoryAllocator)
{
	//Create scene subobjects
	std::vector<std::wstring> sceneTexturesVec;
	CreateSceneMeshMetadata(sceneTexturesVec);

	UINT64 intermediateBufferOffset = 0;

	//Create buffers for model and uniform data
	CreateSceneDataBufferDescs();

	//Load texture images
	LoadTextureImages(device, sceneTexturesVec);

	//Allocate memory for images and buffers
	AllocateTexturesHeap(device, memoryAllocator);
	AllocateBuffersHeap(device, memoryAllocator);

	CreateImageViews();

	//Create intermediate buffers
	VkDeviceSize currentIntermediateBufferSize = intermediateBufferOffset; //The size of current data is the offset to write new data, simple
	CreateIntermediateBuffer(deviceQueues, memoryAllocator, currentIntermediateBufferSize);

	//Fill intermediate buffers
	FillIntermediateBufferData();

	CreateDescriptorPool();
	AllocateDescriptorSets();
	FillDescriptorSets(shaderManager);
}

void D3D12::RenderableSceneBuilder::CreateSceneMeshMetadata(std::vector<std::wstring>& sceneTexturesVec)
{
	mVertexBufferData.clear();
	mIndexBufferData.clear();

	mSceneToBuild->mSceneMeshes.clear();
	mSceneToBuild->mSceneSubobjects.clear();

	std::unordered_map<std::wstring, size_t> textureVecIndices;
	for(auto it = mSceneTextures.begin(); it != mSceneTextures.end(); ++it)
	{
		sceneTexturesVec.push_back(*it);
		textureVecIndices[*it] = sceneTexturesVec.size() - 1;
	}

	//Create scene subobjects
	for(size_t i = 0; i < mSceneMeshes.size(); i++)
	{
		//Need to change this in case of multiple subobjects for mesh
		RenderableScene::MeshSubobjectRange subobjectRange;
		subobjectRange.FirstSubobjectIndex     = (uint32_t)i;
		subobjectRange.AfterLastSubobjectIndex = (uint32_t)(i + 1);

		mSceneToBuild->mSceneMeshes.push_back(subobjectRange);

		RenderableScene::SceneSubobject subobject;
		subobject.FirstIndex                = (uint32_t)mIndexBufferData.size();
		subobject.IndexCount                = (uint32_t)mSceneMeshes[i].Indices.size();
		subobject.VertexOffset              = (int32_t)mVertexBufferData.size();
		subobject.TextureDescriptorSetIndex = (uint32_t)textureVecIndices[mSceneMeshes[i].TextureFilename];

		mVertexBufferData.insert(mVertexBufferData.end(), mSceneMeshes[i].Vertices.begin(), mSceneMeshes[i].Vertices.end());
		mIndexBufferData.insert(mIndexBufferData.end(), mSceneMeshes[i].Indices.begin(), mSceneMeshes[i].Indices.end());

		mSceneMeshes[i].Vertices.clear();
		mSceneMeshes[i].Indices.clear();

		mSceneToBuild->mSceneSubobjects.push_back(subobject);
	}

	mSceneToBuild->mScenePerObjectData.resize(mSceneToBuild->mSceneMeshes.size());
	mSceneToBuild->mScheduledSceneUpdates.resize(mSceneToBuild->mSceneMeshes.size() + 1); //1 for each subobject and 1 for frame data
	mSceneToBuild->mObjectDataScheduledUpdateIndices.resize(mSceneToBuild->mSceneSubobjects.size(), (uint32_t)(-1));
	mSceneToBuild->mFrameDataScheduledUpdateIndex = (uint32_t)(-1);
}

void D3D12::RenderableSceneBuilder::CreateSceneDataBufferDescs()
{
	const UINT64 vertexDataSize = mVertexBufferData.size() * sizeof(RenderableSceneVertex);

	D3D12_RESOURCE_DESC1 vertexBufferDesc;
	vertexBufferDesc.Dimension                       = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexBufferDesc.Alignment                       = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	vertexBufferDesc.Width                           = vertexDataSize;
	vertexBufferDesc.Height                          = 1;
	vertexBufferDesc.DepthOrArraySize                = 1;
	vertexBufferDesc.MipLevels                       = 1;
	vertexBufferDesc.Format                          = DXGI_FORMAT_UNKNOWN;
	vertexBufferDesc.SampleDesc.Count                = 1;
	vertexBufferDesc.SampleDesc.Quality              = 0;
	vertexBufferDesc.Layout                          = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	vertexBufferDesc.Flags                           = D3D12_RESOURCE_FLAG_NONE;
	vertexBufferDesc.SamplerFeedbackMipRegion.Width  = 1;
	vertexBufferDesc.SamplerFeedbackMipRegion.Height = 1;
	vertexBufferDesc.SamplerFeedbackMipRegion.Depth  = 1;


	const UINT64 indexDataSize = mIndexBufferData.size() * sizeof(RenderableSceneIndex);

	D3D12_RESOURCE_DESC1 indexBufferDesc;
	indexBufferDesc.Dimension                       = D3D12_RESOURCE_DIMENSION_BUFFER;
	indexBufferDesc.Alignment                       = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	indexBufferDesc.Width                           = indexDataSize;
	indexBufferDesc.Height                          = 1;
	indexBufferDesc.DepthOrArraySize                = 1;
	indexBufferDesc.MipLevels                       = 1;
	indexBufferDesc.Format                          = DXGI_FORMAT_UNKNOWN;
	indexBufferDesc.SampleDesc.Count                = 1;
	indexBufferDesc.SampleDesc.Quality              = 0;
	indexBufferDesc.Layout                          = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	indexBufferDesc.Flags                           = D3D12_RESOURCE_FLAG_NONE;
	indexBufferDesc.SamplerFeedbackMipRegion.Width  = 1;
	indexBufferDesc.SamplerFeedbackMipRegion.Height = 1;
	indexBufferDesc.SamplerFeedbackMipRegion.Depth  = 1;


	const UINT64 constantPerObjectDataSize = mSceneToBuild->mScenePerObjectData.size() * mSceneToBuild->mGBufferObjectChunkDataSize * D3D12Utils::InFlightFrameCount;
	const UINT64 constantPerFrameDataSize  = mSceneToBuild->mGBufferFrameChunkDataSize * D3D12Utils::InFlightFrameCount;

	D3D12_RESOURCE_DESC1 constantBufferDesc;
	constantBufferDesc.Dimension                       = D3D12_RESOURCE_DIMENSION_BUFFER;
	constantBufferDesc.Alignment                       = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	constantBufferDesc.Width                           = constantPerObjectDataSize + constantPerFrameDataSize;
	constantBufferDesc.Height                          = 1;
	constantBufferDesc.DepthOrArraySize                = 1;
	constantBufferDesc.MipLevels                       = 1;
	constantBufferDesc.Format                          = DXGI_FORMAT_UNKNOWN;
	constantBufferDesc.SampleDesc.Count                = 1;
	constantBufferDesc.SampleDesc.Quality              = 0;
	constantBufferDesc.Layout                          = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	constantBufferDesc.Flags                           = D3D12_RESOURCE_FLAG_NONE;
	constantBufferDesc.SamplerFeedbackMipRegion.Width  = 1;
	constantBufferDesc.SamplerFeedbackMipRegion.Height = 1;
	constantBufferDesc.SamplerFeedbackMipRegion.Depth  = 1;

	mSceneToBuild->mSceneDataConstantObjectBufferOffset = 0;
	mSceneToBuild->mSceneDataConstantFrameBufferOffset = mSceneToBuild->mSceneDataConstantObjectBufferOffset + constantPerObjectDataSize;
}

void D3D12::RenderableSceneBuilder::LoadTextureImages(ID3D12Device* device, const std::vector<std::wstring>& sceneTextures)
{
	mTextureData.clear();
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
		texDesc1.SamplerFeedbackMipRegion.Width  = 1;
		texDesc1.SamplerFeedbackMipRegion.Height = 1;
		texDesc1.SamplerFeedbackMipRegion.Depth  = 1;

		mTextureDescs.push_back(texDesc1);

		texCommited.reset();


		UINT64 textureByteSize = 0;
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(subresources.size());
		std::vector<UINT>                               footprintHeights(subresources.size());
		std::vector<UINT64>                             footprintByteWidths(subresources.size());
		device->GetCopyableFootprints(&texDesc, 0, subresources.size(), 0, footprints.data(), footprintHeights.data(), footprintByteWidths.data(), &textureByteSize);

		size_t currentTextureOffset = mTextureData.size();
		mTextureData.resize(mTextureData.size() + textureByteSize);
		for(size_t j = 0; j < subresources.size(); j++)
		{
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT offsetedFootprint;
			offsetedFootprint.Offset    = currentTextureOffset + footprints[j].Offset;
			offsetedFootprint.Footprint = footprints[j].Footprint;

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
	}
}

void D3D12::RenderableSceneBuilder::AllocateBuffersHeap(ID3D12Device8* device, const MemoryManager* memoryAllocator)
{
	mSceneToBuild->mHeapForGpuBuffers.reset();
	mSceneToBuild->mHeapForCpuVisibleBuffers.reset();

	std::vector gpuBufferDescs = {mVertexBufferDesc, mIndexBufferDesc};

	std::vector<UINT64> gpuBufferOffsets;
	memoryAllocator->AllocateBuffersMemory(device, gpuBufferDescs, MemoryManager::BufferAllocationType::GPU_LOCAL, gpuBufferOffsets, mSceneToBuild->mHeapForGpuBuffers.put());


	std::vector hostVisibleBufferDescs = {mConstantBufferDesc};

	std::vector<UINT64> cpuVisibleBufferOffsets;
	memoryAllocator->AllocateBuffersMemory(device, hostVisibleBufferDescs, MemoryManager::BufferAllocationType::CPU_VISIBLE, cpuVisibleBufferOffsets, mSceneToBuild->mHeapForCpuVisibleBuffers.put());
}

void D3D12::RenderableSceneBuilder::CreateTextures(ID3D12Device8* device, const std::vector<D3D12_RESOURCE_DESC1>& textureDescs)
{
	for(const D3D12_RESOURCE_DESC1& textureDesc: textureDescs)
	{
		THROW_IF_FAILED(device->CreatePlacedResource1(mSceneToBuild->mHeapForTextures, ))
	}
}

void D3D12::RenderableSceneBuilder::AllocateTexturesHeap(ID3D12Device8* device, const MemoryManager* memoryAllocator, std::vector<D3D12_RESOURCE_DESC1>& outTextureDescs)
{
	outTextureDescs.clear();
	mSceneToBuild->mHeapForTextures.reset();

	std::vector<UINT64> textureHeapOffsets;
	memoryAllocator->AllocateTextureMemory(device, outTextureDescs, textureHeapOffsets, mSceneToBuild->mHeapForTextures.put());
}