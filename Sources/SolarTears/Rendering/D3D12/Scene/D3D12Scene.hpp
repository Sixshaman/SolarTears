#pragma once

#include <d3d12.h>
#include <vector>
#include <string>
#include <wil/com.h>
#include "../../Common/RenderingUtils.hpp"
#include "../../Common/Scene/ModernRenderableScene.hpp"
#include "../../../Core/FrameCounter.hpp"

namespace D3D12
{
	class ShaderManager;

	class RenderableScene: public ModernRenderableScene
	{
		friend class RenderableSceneBuilder;
		friend class SceneDescriptorCreator;

	public:
		RenderableScene(const FrameCounter* frameCounter);
		~RenderableScene();

	public:
		void PrepareDrawBuffers(ID3D12GraphicsCommandList* cmdList) const;
		void DrawBakedPositionObjectsWithRootConstants(ID3D12GraphicsCommandList* cmdList, UINT materialIndexRootBinding) const;
		void DrawBufferedPositionObjectsWithRootConstants(ID3D12GraphicsCommandList* cmdList, UINT objectIndexRootBinding, UINT materialIndexRootBinding) const;

	private:
		wil::com_ptr_nothrow<ID3D12Resource> mSceneVertexBuffer;
		wil::com_ptr_nothrow<ID3D12Resource> mSceneIndexBuffer;

		wil::com_ptr_nothrow<ID3D12Resource> mSceneStaticConstantBuffer;  //Common buffer for all static constant buffer data (GPU-local)
		wil::com_ptr_nothrow<ID3D12Resource> mSceneDynamicConstantBuffer; //Common buffer for all dynamic constant buffer data (CPU-visible)

		D3D12_VERTEX_BUFFER_VIEW mSceneVertexBufferView;
		D3D12_INDEX_BUFFER_VIEW  mSceneIndexBufferView;

		std::vector<wil::com_ptr_nothrow<ID3D12Resource2>> mSceneTextures;

		wil::com_ptr_nothrow<ID3D12Heap> mHeapForGpuBuffers;
		wil::com_ptr_nothrow<ID3D12Heap> mHeapForCpuVisibleBuffers;
		wil::com_ptr_nothrow<ID3D12Heap> mHeapForTextures;
	};
}