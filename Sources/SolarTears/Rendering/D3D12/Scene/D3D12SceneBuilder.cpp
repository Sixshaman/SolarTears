#include "D3D12SceneBuilder.hpp"
#include "../D3D12Utils.hpp"
#include "../D3D12Memory.hpp"
#include "../D3D12DeviceQueues.hpp"
#include "../D3D12WorkerCommandLists.hpp"
#include "../../../../3rd party/DirectXTex/DDSTextureLoader/DDSTextureLoader12.h"
#include <array>
#include <cassert>
#include <vector>

D3D12::RenderableSceneBuilder::RenderableSceneBuilder(RenderableScene* sceneToBuild): mSceneToBuild(sceneToBuild), mVertexBufferDesc({}), mIndexBufferDesc({}), mConstantBufferDesc({}),
                                                                                      mVertexBufferHeapOffset(0), mIndexBufferHeapOffset(0), mConstantBufferHeapOffset(0),
	                                                                                  mIntermediateBufferVertexDataOffset(0), mIntermediateBufferIndexDataOffset(0), mIntermediateBufferTextureDataOffset(0)
{
	assert(sceneToBuild != nullptr);
}

D3D12::RenderableSceneBuilder::~RenderableSceneBuilder()
{
}

void D3D12::RenderableSceneBuilder::BakeSceneFirstPart(ID3D12Device8* device, const MemoryManager* memoryAllocator)
{
	//Create scene subobjects
	std::vector<std::wstring> sceneTexturesVec;
	CreateSceneMeshMetadata(sceneTexturesVec);

	UINT64 intermediateBufferSize = 0;

	//Create buffers for model and uniform data
	intermediateBufferSize = CreateSceneDataBufferDescs(intermediateBufferSize);

	//Load texture images
	intermediateBufferSize = LoadTextureImages(device, sceneTexturesVec, intermediateBufferSize);

	//Allocate memory for images and buffers
	AllocateTexturesHeap(device, memoryAllocator);
	AllocateBuffersHeap(device, memoryAllocator);

	CreateBuffers(device);
	CreateTextures(device);

	//Create intermediate buffers
	CreateIntermediateBuffer(device, intermediateBufferSize);

	//Fill intermediate buffers
	FillIntermediateBufferData();
}

void D3D12::RenderableSceneBuilder::BakeSceneSecondPart(DeviceQueues* deviceQueues, const WorkerCommandLists* commandLists)
{
	ID3D12CommandAllocator*    commandAllocator = commandLists->GetMainThreadDirectCommandAllocator(0);
	ID3D12GraphicsCommandList* commandList      = commandLists->GetMainThreadDirectCommandList();

	//Baking
	THROW_IF_FAILED(commandAllocator->Reset());
	THROW_IF_FAILED(commandList->Reset(commandAllocator, nullptr));
	
	for(size_t i = 0; i < mSceneToBuild->mSceneTextures.size(); i++)
	{
		for(size_t j = 0; j < mSceneTextureSubresourceFootprints[i].size(); j++)
		{
			D3D12_TEXTURE_COPY_LOCATION dstLocation;
			dstLocation.pResource        = mSceneToBuild->mSceneTextures[i].get();
			dstLocation.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dstLocation.SubresourceIndex = (UINT)j;

			D3D12_TEXTURE_COPY_LOCATION srcLocation;
			srcLocation.pResource       = mIntermediateBuffer.get();
			srcLocation.Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			srcLocation.PlacedFootprint = mSceneTextureSubresourceFootprints[i][j];

			commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);
		}
	}


	std::array sceneBuffers           = {mSceneToBuild->mSceneVertexBuffer.get(),                  mSceneToBuild->mSceneIndexBuffer.get()};
	std::array sceneBufferDataOffsets = {mIntermediateBufferVertexDataOffset,                      mIntermediateBufferIndexDataOffset};
	std::array sceneBufferDataSizes   = {mVertexBufferData.size() * sizeof(RenderableSceneVertex), mIndexBufferData.size() * sizeof(RenderableSceneIndex)};
	std::array sceneBufferStates      = {D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,          D3D12_RESOURCE_STATE_INDEX_BUFFER};
	for(size_t i = 0; i < sceneBuffers.size(); i++)
	{
		commandList->CopyBufferRegion(sceneBuffers[i], 0, mIntermediateBuffer.get(), sceneBufferDataOffsets[i], sceneBufferDataSizes[i]);
	}


	std::vector<D3D12_RESOURCE_BARRIER> barriers;
	barriers.reserve(mSceneToBuild->mSceneTextures.size() + sceneBuffers.size());
	for(int i = 0; i < mSceneToBuild->mSceneTextures.size(); i++)
	{
		D3D12_RESOURCE_BARRIER textureBarrier;
		textureBarrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		textureBarrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		textureBarrier.Transition.pResource   = mSceneToBuild->mSceneTextures[i].get();
		textureBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		textureBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		textureBarrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		barriers.push_back(textureBarrier);
	}

	for(int i = 0; i < sceneBuffers.size(); i++)
	{
		D3D12_RESOURCE_BARRIER textureBarrier;
		textureBarrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		textureBarrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		textureBarrier.Transition.pResource   = sceneBuffers[i];
		textureBarrier.Transition.Subresource = 0;
		textureBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		textureBarrier.Transition.StateAfter  = sceneBufferStates[i];

		barriers.push_back(textureBarrier);
	}

	commandList->ResourceBarrier((UINT)barriers.size(), barriers.data());


	THROW_IF_FAILED(commandList->Close());

	ID3D12CommandQueue* graphicsQueue = deviceQueues->GraphicsQueueHandle();

	std::array commandListsToExecute = {reinterpret_cast<ID3D12CommandList*>(commandList)};
	graphicsQueue->ExecuteCommandLists((UINT)commandListsToExecute.size(), commandListsToExecute.data());

	UINT64 waitFenceValue = deviceQueues->GraphicsFenceSignal();
	deviceQueues->GraphicsQueueCpuWait(waitFenceValue);
}

void D3D12::RenderableSceneBuilder::CreateSceneMeshMetadata(std::vector<std::wstring>& sceneTexturesVec)
{
	mVertexBufferData.clear();
	mIndexBufferData.clear();

	mSceneToBuild->mSceneMeshes.clear();
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

UINT64 D3D12::RenderableSceneBuilder::CreateSceneDataBufferDescs(UINT64 currentIntermediateBufferSize)
{
	UINT64 intermediateBufferSize = currentIntermediateBufferSize;


	const UINT64 vertexDataSize = mVertexBufferData.size() * sizeof(RenderableSceneVertex);

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

	mIntermediateBufferVertexDataOffset = intermediateBufferSize;
	intermediateBufferSize             += vertexDataSize;


	const UINT64 indexDataSize = mIndexBufferData.size() * sizeof(RenderableSceneIndex);

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

	mIntermediateBufferIndexDataOffset = intermediateBufferSize;
	intermediateBufferSize            += indexDataSize;


	const UINT64 constantPerObjectDataSize = mSceneToBuild->mScenePerObjectData.size() * mSceneToBuild->mGBufferObjectChunkDataSize * D3D12Utils::InFlightFrameCount;
	const UINT64 constantPerFrameDataSize  = mSceneToBuild->mGBufferFrameChunkDataSize * D3D12Utils::InFlightFrameCount;

	mConstantBufferDesc.Dimension                       = D3D12_RESOURCE_DIMENSION_BUFFER;
	mConstantBufferDesc.Alignment                       = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	mConstantBufferDesc.Width                           = constantPerObjectDataSize + constantPerFrameDataSize;
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

	mSceneToBuild->mSceneDataConstantObjectBufferOffset = 0;
	mSceneToBuild->mSceneDataConstantFrameBufferOffset = mSceneToBuild->mSceneDataConstantObjectBufferOffset + constantPerObjectDataSize;

	return intermediateBufferSize;
}

UINT64 D3D12::RenderableSceneBuilder::LoadTextureImages(ID3D12Device8* device, const std::vector<std::wstring>& sceneTextures, UINT64 currentIntermediateBufferSize)
{
	mIntermediateBufferTextureDataOffset = D3D12Utils::AlignMemory(currentIntermediateBufferSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
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

void D3D12::RenderableSceneBuilder::AllocateBuffersHeap(ID3D12Device8* device, const MemoryManager* memoryAllocator)
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

void D3D12::RenderableSceneBuilder::AllocateTexturesHeap(ID3D12Device8* device, const MemoryManager* memoryAllocator)
{
	mSceneTextureHeapOffsets.clear();
	mSceneToBuild->mHeapForTextures.reset();

	memoryAllocator->AllocateTextureMemory(device, mSceneTextureDescs, mSceneTextureHeapOffsets, mSceneToBuild->mHeapForTextures.put());
}

void D3D12::RenderableSceneBuilder::CreateTextures(ID3D12Device8* device)
{
	assert(mSceneTextureDescs.size() == mSceneTextureHeapOffsets.size());

	mSceneToBuild->mSceneTextures.reserve(mSceneTextureDescs.size());
	for(size_t i = 0; i < mSceneTextureDescs.size(); i++)
	{
		mSceneToBuild->mSceneTextures.emplace_back();
		THROW_IF_FAILED(device->CreatePlacedResource1(mSceneToBuild->mHeapForTextures.get(), mSceneTextureHeapOffsets[i], &mSceneTextureDescs[i], D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(mSceneToBuild->mSceneTextures.back().put())));
	}
}

void D3D12::RenderableSceneBuilder::CreateBuffers(ID3D12Device8* device)
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

void D3D12::RenderableSceneBuilder::CreateIntermediateBuffer(ID3D12Device8* device, UINT64 intermediateBufferSize)
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

	THROW_IF_FAILED(device->CreateCommittedResource2(&intermediateBufferHeapProperties, D3D12_HEAP_FLAG_NONE, &intermediateBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, nullptr, IID_PPV_ARGS(mIntermediateBuffer.put())));
}

void D3D12::RenderableSceneBuilder::FillIntermediateBufferData()
{
	const UINT64 textureDataSize = mTextureData.size()      * sizeof(uint8_t);
	const UINT64 vertexDataSize  = mVertexBufferData.size() * sizeof(RenderableSceneVertex);
	const UINT64 indexDataSize   = mIndexBufferData.size()  * sizeof(RenderableSceneIndex);

	D3D12_RANGE readRange;
	readRange.Begin = 0;
	readRange.End   = 0;

	void* bufferData = nullptr;
	THROW_IF_FAILED(mIntermediateBuffer->Map(0, &readRange, &bufferData));

	std::byte* bufferDataBytes = reinterpret_cast<std::byte*>(bufferData);
	memcpy(bufferDataBytes + mIntermediateBufferVertexDataOffset,  mVertexBufferData.data(), vertexDataSize);
	memcpy(bufferDataBytes + mIntermediateBufferIndexDataOffset,   mIndexBufferData.data(),  indexDataSize);
	memcpy(bufferDataBytes + mIntermediateBufferTextureDataOffset, mTextureData.data(),      textureDataSize);

	mIntermediateBuffer->Unmap(0, nullptr);
}