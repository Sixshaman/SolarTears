#pragma once

#include <d3d12.h>
#include <wil/com.h>

namespace D3D12
{
	class SceneDescriptorCreator;
	class FrameGraphDescriptorCreator;

	class SrvDescriptorManager
	{
	public:
		static constexpr uint32_t FLAG_SCENE_UNCHANGED       = 0x1;
		static constexpr uint32_t FLAG_FRAME_GRAPH_UNCHANGED = 0x2;

	public:
		SrvDescriptorManager(ID3D12Device* device);
		~SrvDescriptorManager();

		void ValidateDescriptorHeaps(ID3D12Device* device, SceneDescriptorCreator* sceneDescriptorCreator, FrameGraphDescriptorCreator* frameGraphDescriptorCreator, uint32_t flags);

	private:
		wil::com_ptr_nothrow<ID3D12DescriptorHeap> mSrvUavCbvDescriptorHeap;

		UINT mSceneSrvDescriptorCount;
		UINT mFrameGraphSrvDescriptorCount;
	};
}