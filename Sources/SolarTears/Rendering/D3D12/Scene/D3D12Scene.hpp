#pragma once

#include <d3d12.h>
#include <vector>
#include <string>
#include <wil/com.h>
#include "../../RenderableSceneBase.hpp"
#include "../../../Core/FrameCounter.hpp"

namespace D3D12
{
	class ShaderManager;

	class RenderableScene: public RenderableSceneBase
	{
		friend class RenderableSceneBuilder;
		friend class SceneDescriptorCreator;

		struct SceneSubobject
		{
			uint32_t IndexCount;
			uint32_t FirstIndex;
			int32_t  VertexOffset;
			uint32_t TextureDescriptorSetIndex;

			//Material data will be set VIA ROOT CONSTANTS
		};

		struct MeshSubobjectRange
		{
			uint32_t FirstSubobjectIndex;
			uint32_t AfterLastSubobjectIndex;
		};

	public:
		RenderableScene(const FrameCounter* frameCounter);
		~RenderableScene();

		void FinalizeSceneUpdating() override;

	public:
		void DrawObjectsOntoGBuffer(ID3D12GraphicsCommandList* commandList, const ShaderManager* shaderManager) const;

	private:
		//Created from inside
		const FrameCounter* mFrameCounterRef;

		uint32_t mGBufferObjectChunkDataSize;
		uint32_t mGBufferFrameChunkDataSize;

		UINT64 CalculatePerObjectDataOffset(uint32_t objectIndex, uint32_t currentFrameResourceIndex) const;
		UINT64 CalculatePerFrameDataOffset(uint32_t currentFrameResourceIndex)                        const;

	private:
		//Created from outside
		std::vector<MeshSubobjectRange> mSceneMeshes;
		std::vector<SceneSubobject>     mSceneSubobjects;

		void* mSceneConstantDataBufferPointer; //Constant buffer is persistently mapped

		wil::com_ptr_nothrow<ID3D12Resource> mSceneVertexBuffer;
		wil::com_ptr_nothrow<ID3D12Resource> mSceneIndexBuffer;

		D3D12_VERTEX_BUFFER_VIEW mSceneVertexBufferView;
		D3D12_INDEX_BUFFER_VIEW  mSceneIndexBufferView;

		wil::com_ptr_nothrow<ID3D12Resource> mSceneConstantBuffer; //Common buffer for all constant buffer data

		std::vector<wil::com_ptr_nothrow<ID3D12Resource>> mSceneTextures;
		std::vector<D3D12_GPU_DESCRIPTOR_HANDLE>          mSceneTextureDescriptors;

		wil::com_ptr_nothrow<ID3D12Heap> mHeapForGpuBuffers;
		wil::com_ptr_nothrow<ID3D12Heap> mHeapForCpuVisibleBuffers;
		wil::com_ptr_nothrow<ID3D12Heap> mHeapForTextures;

		UINT64 mSceneDataConstantObjectBufferOffset;
		UINT64 mSceneDataConstantFrameBufferOffset;
	};
}