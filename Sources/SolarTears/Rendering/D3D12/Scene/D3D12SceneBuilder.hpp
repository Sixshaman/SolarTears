#pragma once

#include <d3d12.h>
#include <vector>
#include <string>
#include <memory>
#include "D3D12Scene.hpp"
#include "../D3D12Utils.hpp"
#include "../D3D12DeviceQueues.hpp"
#include "../../RenderableSceneBuilderBase.hpp"

namespace D3D12
{
	class RenderableSceneBuilder: public RenderableSceneBuilderBase
	{
	public:
		RenderableSceneBuilder(RenderableScene* sceneToBuild);
		~RenderableSceneBuilder();

		void BakeSceneFirstPart(ID3D12Device8* device, const DeviceQueues* deviceQueues, const MemoryManager* memoryAllocator, const ShaderManager* shaderManager);
		void BakeSceneSecondPart(DeviceQueues* deviceQueues, WorkerCommandBuffers* workerCommandBuffers);

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

		void CreateSceneDataBufferDescs();
		void LoadTextureImages(ID3D12Device* device, const std::vector<std::wstring>& sceneTextures);

	private:
		RenderableScene* mSceneToBuild;

		std::vector<D3D12_RESOURCE_DESC1> mSceneTextureDescs;

		std::vector<uint8_t>               mTextureData;
		std::vector<RenderableSceneVertex> mVertexBufferData;
		std::vector<RenderableSceneIndex>  mIndexBufferData;

		D3D12_RESOURCE_DESC1 mVertexBufferDesc;
		D3D12_RESOURCE_DESC1 mIndexBufferDesc;
		D3D12_RESOURCE_DESC1 mConstantBufferDesc;
		
		std::vector<D3D12_RESOURCE_DESC> mTextureDescs;

		wil::com_ptr_nothrow<ID3D12Resource> mIntermediateBuffer;
		wil::com_ptr_nothrow<ID3D12Heap1>    mIntermediateBufferHeap;

		UINT64 mIntermediateBufferVertexDataOffset;
		UINT64 mIntermediateBufferIndexDataOffset;
		UINT64 mIntermediateBufferTextureDataOffset;
	};
}

#include "D3D12SceneBuilder.inl"