#pragma once

#include <d3d12.h>
#include <vector>
#include <string>
#include <memory>
#include "D3D12Scene.hpp"
#include "../D3D12Utils.hpp"
#include "../../Common/Scene/ModernRenderableSceneBuilder.hpp"

namespace D3D12
{
	class MemoryManager;
	class WorkerCommandLists;
	class DeviceQueues;

	class RenderableSceneBuilder: public ModernRenderableSceneBuilder
	{
	public:
		RenderableSceneBuilder(ID3D12Device8* device, RenderableScene* sceneToBuild);
		~RenderableSceneBuilder();

	protected:
		void CreateVertexBufferPrerequisiteObject(size_t vertexDataSize)                        override final;
		void CreateIndexBufferPrerequisiteObject(size_t indexDataSize)                          override final;
		void CreateConstantBufferPrerequisiteObject(size_t constantDataSize)                    override final;
		size_t CreateTexturePrerequisiteObjects(const std::vector<std::wstring>& sceneTextures) override final;

		virtual void       CreateIntermediateBuffer(uint64_t intermediateBufferSize) override final;
		virtual std::byte* MapIntermediateBuffer()                                   const override final;
		virtual void       UnmapIntermediateBuffer()                                 const override final;

	private:
		void AllocateTexturesHeap(const MemoryManager* memoryAllocator);
		void AllocateBuffersHeap(const MemoryManager* memoryAllocator);

		void CreateTextures();
		void CreateBuffers();

		void CreateBufferViews();

	private:
		ID3D12Device8* mDeviceRef;

		RenderableScene* mD3d12SceneToBuild;

		D3D12_RESOURCE_DESC1 mVertexBufferDesc;
		D3D12_RESOURCE_DESC1 mIndexBufferDesc;
		D3D12_RESOURCE_DESC1 mConstantBufferDesc;

		std::vector<D3D12_RESOURCE_DESC1>                            mSceneTextureDescs;
		std::vector<UINT64>                                          mSceneTextureHeapOffsets;
		std::vector<std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT>> mSceneTextureSubresourceFootprints;

		wil::com_ptr_nothrow<ID3D12Resource> mIntermediateBuffer;
	};
}

#include "D3D12SceneBuilder.inl"