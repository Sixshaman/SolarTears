#pragma once

#include <d3d12.h>
#include <wil/com.h>
#include "D3D12DescriptorManager.hpp"

namespace D3D12
{
	class SceneDescriptorCreator;
	class FrameGraphDescriptorCreator;

	class DescriptorManager
	{
	public:
		static constexpr uint32_t FLAG_SCENE_UNCHANGED       = 1;
		static constexpr uint32_t FLAG_FRAME_GRAPH_UNCHANGED = 2;

	public:
		DescriptorManager(ID3D12Device* device);
		~DescriptorManager();

		void ValidateDescriptorHeaps(ID3D12Device* device, SceneDescriptorCreator* sceneDescriptorCreator, FrameGraphDescriptorCreator* frameGraphDescriptorCreator, uint32_t flags);

	private:
		wil::com_ptr_nothrow<ID3D12DescriptorHeap> mSrvUavCbvDescriptorHeap;
		wil::com_ptr_nothrow<ID3D12DescriptorHeap> mRtvDescriptorHeap;
		wil::com_ptr_nothrow<ID3D12DescriptorHeap> mDsvDescriptorHeap;

		UINT mSceneSrvDescriptorCount;
		UINT mFrameGraphSrvDescriptorCount;
		UINT mRtvDescriptorCount;
		UINT mDsvDescriptorCount;

		UINT mSrvUavCbvDescriptorSize;
		UINT mRtvDescriptorSize;
		UINT mDsvDescriptorSize;
	};
}