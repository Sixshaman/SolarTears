#include "D3D12SrvDescriptorManager.hpp"

D3D12::SrvDescriptorManager::SrvDescriptorManager()
{
}

D3D12::SrvDescriptorManager::~SrvDescriptorManager()
{
}

ID3D12DescriptorHeap* D3D12::SrvDescriptorManager::GetDescriptorHeap() const
{
	return mSrvUavCbvDescriptorHeap.get();
}

void D3D12::SrvDescriptorManager::ValidateDescriptorHeaps(ID3D12Device* device, UINT newDescriptorCount)
{
	if(newDescriptorCount > mSrvUavCbvDescriptorHeap->GetDesc().NumDescriptors) //Need to recreate the whole descriptor heap
	{
		mSrvUavCbvDescriptorHeap.reset();

		//TODO: mGPU?
		D3D12_DESCRIPTOR_HEAP_DESC srvDescriptorHeapDesc;
		srvDescriptorHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvDescriptorHeapDesc.NumDescriptors = newDescriptorCount;
		srvDescriptorHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		srvDescriptorHeapDesc.NodeMask       = 0;

		THROW_IF_FAILED(device->CreateDescriptorHeap(&srvDescriptorHeapDesc, IID_PPV_ARGS(mSrvUavCbvDescriptorHeap.put())));
	}
}