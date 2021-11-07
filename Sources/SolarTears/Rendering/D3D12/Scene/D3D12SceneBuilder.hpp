#pragma once

#include <d3d12.h>
#include <vector>
#include <string>
#include <memory>
#include "D3D12Scene.hpp"
#include "../D3D12Utils.hpp"
#include "../../Common/Scene/ModernRenderableSceneBuilder.hpp"
#include "../../../Core/DataStructures/Span.hpp"

namespace D3D12
{
	class MemoryManager;
	class WorkerCommandLists;
	class DeviceQueues;

	class RenderableSceneBuilder: public ModernRenderableSceneBuilder
	{
	public:
		RenderableSceneBuilder(ID3D12Device8* device, RenderableScene* sceneToBuild, MemoryManager* memoryAllocator, DeviceQueues* deviceQueues, const WorkerCommandLists* commandLists);
		~RenderableSceneBuilder();

	protected:
		void CreateVertexBufferInfo(size_t vertexDataSize)     override final;
		void CreateIndexBufferInfo(size_t indexDataSize)       override final;
		void CreateConstantBufferInfo(size_t constantDataSize) override final;
		void CreateUploadBufferInfo(size_t uploadDataSize)     override final;

		void AllocateTextureMetadataArrays(size_t textureCount)                                                                                                              override final;
		void LoadTextureFromFile(const std::wstring& textureFilename, uint64_t currentIntermediateBufferOffset, size_t textureIndex, std::vector<std::byte>& outTextureData) override final;

		virtual void FinishBufferCreation()  override final;
		virtual void FinishTextureCreation() override final;

		virtual std::byte* MapUploadBuffer() override final;

		virtual void       CreateIntermediateBuffer()      override final;
		virtual std::byte* MapIntermediateBuffer()   const override final;
		virtual void       UnmapIntermediateBuffer() const override final;

		virtual void WriteInitializationCommands()   const override final;
		virtual void SubmitInitializationCommands()  const override final;
		virtual void WaitForInitializationCommands() const override final;

	private:
		void AllocateBuffersHeap();

		void CreateBufferObjects();
		void CreateBufferViews();

	private:
		ID3D12Device8* mDeviceRef;

		RenderableScene* mD3d12SceneToBuild;

		MemoryManager*            mMemoryAllocator;
		DeviceQueues*             mDeviceQueues;
		const WorkerCommandLists* mWorkerCommandLists;

		D3D12_RESOURCE_DESC1 mVertexBufferDesc;
		D3D12_RESOURCE_DESC1 mIndexBufferDesc;
		D3D12_RESOURCE_DESC1 mConstantBufferDesc;
		D3D12_RESOURCE_DESC1 mUploadBufferDesc;

		std::vector<D3D12_RESOURCE_DESC1>               mSceneTextureDescs;
		std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> mSceneTextureSubresourceFootprints;
		std::vector<Span<uint32_t>>                     mSceneTextureSubresourceSpans;

		wil::com_ptr_nothrow<ID3D12Resource> mIntermediateBuffer;
	};
}