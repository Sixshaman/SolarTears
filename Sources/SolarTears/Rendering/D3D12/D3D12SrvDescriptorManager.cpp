#include "D3D12SrvDescriptorManager.hpp"
#include "Scene/D3D12SceneDescriptorCreator.hpp"
#include "FrameGraph/D3D12FrameGraphDescriptorCreator.hpp"

D3D12::SrvDescriptorManager::SrvDescriptorManager(): mSceneHeapPortionSize(0), mFrameGraphHeapPortionSize(0)
{
}

D3D12::SrvDescriptorManager::~SrvDescriptorManager()
{
}

ID3D12DescriptorHeap* D3D12::SrvDescriptorManager::GetDescriptorHeap() const
{
	return mSrvUavCbvDescriptorHeap.get();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::SrvDescriptorManager::GetSceneCpuDescriptorStart() const
{
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHeapStart = mSrvUavCbvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	return cpuHeapStart;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12::SrvDescriptorManager::GetSceneGpuDescriptorStart() const
{
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHeapStart = mSrvUavCbvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	return gpuHeapStart;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12::SrvDescriptorManager::GetFrameGraphCpuDescriptorStart() const
{
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHeapStart = mSrvUavCbvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	cpuHeapStart.ptr += mSceneHeapPortionSize;
	return cpuHeapStart;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12::SrvDescriptorManager::GetFrameGraphGpuDescriptorStart() const
{
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHeapStart = mSrvUavCbvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	gpuHeapStart.ptr += mSceneHeapPortionSize;
	return gpuHeapStart;
}

void D3D12::SrvDescriptorManager::ValidateDescriptorHeaps(ID3D12Device* device, UINT64 newSceneDescriptorCount, UINT64 newFrameGraphDescriptorCount)
{
	UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	UINT newScenePortionSize      = newSceneDescriptorCount * descriptorSize;
	UINT newFrameGraphPortionSize = newFrameGraphDescriptorCount * descriptorSize;
	if(newScenePortionSize + newFrameGraphPortionSize > mSceneHeapPortionSize + mFrameGraphHeapPortionSize) //Need to recreate the whole descriptor heap
	{
		//TODO: mGPU?
		D3D12_DESCRIPTOR_HEAP_DESC srvDescriptorHeapDesc;
		srvDescriptorHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvDescriptorHeapDesc.NumDescriptors = newScenePortionSize + newFrameGraphPortionSize;
		srvDescriptorHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		srvDescriptorHeapDesc.NodeMask       = 0;

		wil::com_ptr_nothrow<ID3D12DescriptorHeap> newSrvDescriptorHeap;
		THROW_IF_FAILED(device->CreateDescriptorHeap(&srvDescriptorHeapDesc, IID_PPV_ARGS(newSrvDescriptorHeap.put())));
	}

	mSceneHeapPortionSize      = newScenePortionSize;
	mFrameGraphHeapPortionSize = newFrameGraphPortionSize;
}