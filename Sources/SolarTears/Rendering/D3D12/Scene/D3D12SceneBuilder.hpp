#pragma once

#include <d3d12.h>
#include <vector>
#include <string>
#include <memory>
#include "D3D12Scene.hpp"
#include "../D3D12Utils.hpp"
#include "../../RenderableSceneBuilderBase.hpp"

namespace D3D12
{
	class MemoryManager;
	class WorkerCommandLists;
	class DeviceQueues;

	class RenderableSceneBuilder: public RenderableSceneBuilderBase
	{
	public:
		RenderableSceneBuilder(RenderableScene* sceneToBuild);
		~RenderableSceneBuilder();

		void BakeSceneFirstPart(ID3D12Device8* device, const MemoryManager* memoryAllocator);
		void BakeSceneSecondPart(DeviceQueues* deviceQueues, const WorkerCommandLists* commandBuffers);

	public:
		static constexpr size_t GetVertexSize();

		static constexpr DXGI_FORMAT GetVertexPositionFormat();
		static constexpr DXGI_FORMAT GetVertexNormalFormat();
		static constexpr DXGI_FORMAT GetVertexTexcoordFormat();

		static constexpr uint32_t GetVertexPositionOffset();
		static constexpr uint32_t GetVertexNormalOffset();
		static constexpr uint32_t GetVertexTexcoordOffset();

	private:
		void CreateSceneMeshMetadata(std::vector<std::wstring>& sceneTexturesVec);

		UINT64 CreateSceneDataBufferDescs(UINT64 currentIntermediateBufferSize);
		UINT64 LoadTextureImages(ID3D12Device8* device, const std::vector<std::wstring>& sceneTextures, UINT64 currentIntermediateBufferSize);

		void AllocateTexturesHeap(ID3D12Device8* device, const MemoryManager* memoryAllocator);
		void AllocateBuffersHeap(ID3D12Device8* device, const MemoryManager* memoryAllocator);

		void CreateTextures(ID3D12Device8* device);
		void CreateBuffers(ID3D12Device8* device);

		void CreateIntermediateBuffer(ID3D12Device8* device, UINT64 intermediateBufferSize);
		void FillIntermediateBufferData();

	private:
		RenderableScene* mSceneToBuild;

		std::vector<uint8_t>               mTextureData;
		std::vector<RenderableSceneVertex> mVertexBufferData;
		std::vector<RenderableSceneIndex>  mIndexBufferData;

		D3D12_RESOURCE_DESC1 mVertexBufferDesc;
		D3D12_RESOURCE_DESC1 mIndexBufferDesc;
		D3D12_RESOURCE_DESC1 mConstantBufferDesc;

		UINT64 mVertexBufferHeapOffset;
		UINT64 mIndexBufferHeapOffset;
		UINT64 mConstantBufferHeapOffset;

		std::vector<D3D12_RESOURCE_DESC1>                            mSceneTextureDescs;
		std::vector<UINT64>                                          mSceneTextureHeapOffsets;
		std::vector<std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT>> mSceneTextureSubresourceFootprints;

		wil::com_ptr_nothrow<ID3D12Resource> mIntermediateBuffer;

		UINT64 mIntermediateBufferVertexDataOffset;
		UINT64 mIntermediateBufferIndexDataOffset;
		UINT64 mIntermediateBufferTextureDataOffset;
	};
}

#include "D3D12SceneBuilder.inl"