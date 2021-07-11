#include "D3D12FrameGraphDescriptorCreator.hpp"

D3D12::FrameGraphDescriptorCreator::FrameGraphDescriptorCreator(FrameGraph* frameGraph): mFrameGraphToCreateDescriptors(frameGraph)
{
}

D3D12::FrameGraphDescriptorCreator::~FrameGraphDescriptorCreator()
{
}

UINT D3D12::FrameGraphDescriptorCreator::GetSrvUavDescriptorCountNeeded()
{
	if(mFrameGraphToCreateDescriptors == nullptr)
	{
		//An optimization to not recreate descriptor heap and all descriptors in it
		//after creating the frame graph, if descriptor heap is already created
		return 10;
	}

	return mFrameGraphToCreateDescriptors->mSrvUavDescriptorCount;
}

void D3D12::FrameGraphDescriptorCreator::RecreateFrameGraphSrvUavDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, D3D12_GPU_DESCRIPTOR_HANDLE startDescriptorGpu, D3D12_GPU_DESCRIPTOR_HANDLE prevStartDescriptorGpu)
{
	if(mFrameGraphToCreateDescriptors == nullptr)
	{
		return;
	}

	if(mFrameGraphToCreateDescriptors->mSrvUavDescriptorCount == 0)
	{
		return;
	}

	device->CopyDescriptorsSimple(mFrameGraphToCreateDescriptors->mSrvUavDescriptorCount, mFrameGraphToCreateDescriptors->mSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), startDescriptorCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	for(size_t i = 0; i < mFrameGraphToCreateDescriptors->mRenderPasses.size(); i++)
	{
		mFrameGraphToCreateDescriptors->mRenderPasses[i]->RevalidateSrvUavDescriptors(prevStartDescriptorGpu, startDescriptorGpu);
	}
}