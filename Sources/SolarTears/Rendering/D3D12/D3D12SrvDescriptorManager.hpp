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
		SrvDescriptorManager();
		~SrvDescriptorManager();

		ID3D12DescriptorHeap* GetDescriptorHeap() const;

		D3D12_CPU_DESCRIPTOR_HANDLE GetSceneCpuDescriptorStart() const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetSceneGpuDescriptorStart() const;

		D3D12_CPU_DESCRIPTOR_HANDLE GetFrameGraphCpuDescriptorStart() const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetFrameGraphGpuDescriptorStart() const;

		void ValidateDescriptorHeaps(ID3D12Device* device, UINT64 newSceneDescriptorCount, UINT64 newFrameGraphDescriptorCount);

	private:
		wil::com_ptr_nothrow<ID3D12DescriptorHeap> mSrvUavCbvDescriptorHeap;

		UINT64 mSceneHeapPortionSize;
		UINT64 mFrameGraphHeapPortionSize;
	};
}