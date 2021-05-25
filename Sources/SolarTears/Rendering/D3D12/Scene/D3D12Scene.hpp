#pragma once

#include <d3d12.h>
#include <vector>
#include <string>
#include <wil/com.h>
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
		void DrawObjectsOntoGBuffer(ID3D12GraphicsCommandList* commandList, const ShaderManager* shaderManager) const;

	private:
		wil::com_ptr_nothrow<ID3D12Resource> mSceneVertexBuffer;
		wil::com_ptr_nothrow<ID3D12Resource> mSceneIndexBuffer;

		wil::com_ptr_nothrow<ID3D12Resource> mSceneConstantBuffer; //Common buffer for all constant buffer data

		D3D12_VERTEX_BUFFER_VIEW mSceneVertexBufferView;
		D3D12_INDEX_BUFFER_VIEW  mSceneIndexBufferView;

		std::vector<wil::com_ptr_nothrow<ID3D12Resource2>> mSceneTextures;
		std::vector<D3D12_GPU_DESCRIPTOR_HANDLE>           mSceneTextureDescriptors;

		wil::com_ptr_nothrow<ID3D12Heap> mHeapForGpuBuffers;
		wil::com_ptr_nothrow<ID3D12Heap> mHeapForCpuVisibleBuffers;
		wil::com_ptr_nothrow<ID3D12Heap> mHeapForTextures;
	};
}