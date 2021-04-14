#pragma once

#include <d3d12.h>
#include <vector>
#include <string>
#include <wil/com.h>
#include "../../RenderableSceneBase.hpp"
#include "../../../Core/FrameCounter.hpp"

namespace D3D12
{
	class RenderableScene: public RenderableSceneBase
	{
		friend class RenderableSceneBuilder;

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

	private:
		//Created from inside
		const FrameCounter* mFrameCounterRef;

		uint32_t mGBufferObjectChunkDataSize;
		uint32_t mGBufferFrameChunkDataSize;

		UINT64 CalculatePerObjectDataOffset(uint32_t objectIndex, uint32_t currentFrameResourceIndex);
		UINT64 CalculatePerFrameDataOffset(uint32_t currentFrameResourceIndex);

	private:
		//Created from outside
		std::vector<MeshSubobjectRange> mSceneMeshes;
		std::vector<SceneSubobject>     mSceneSubobjects;

		void* mSceneConstantDataBufferPointer; //Constant buffer is persistently mapped

		wil::com_ptr_nothrow<ID3D12Resource> mSceneVertexBuffer;
		wil::com_ptr_nothrow<ID3D12Resource> mSceneIndexBuffer;

		wil::com_ptr_nothrow<ID3D12Resource> mSceneConstantBuffer; //Common buffer for all constant buffer data

		std::vector<wil::com_ptr_nothrow<ID3D12Resource>> mSceneTextures;

		wil::com_ptr_nothrow<ID3D12Heap> mHeapForGpuBuffers;
		wil::com_ptr_nothrow<ID3D12Heap> mHeapForCpuVisibleBuffers;
		wil::com_ptr_nothrow<ID3D12Heap> mHeapForTextures;

		UINT64 mSceneDataConstantObjectBufferOffset;
		UINT64 mSceneDataConstantFrameBufferOffset;
	};
}