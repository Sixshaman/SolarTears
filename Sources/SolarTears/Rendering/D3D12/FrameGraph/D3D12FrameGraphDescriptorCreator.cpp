#include "D3D12FrameGraphDescriptorCreator.hpp"

D3D12::FrameGraphDescriptorCreator::FrameGraphDescriptorCreator(FrameGraph* frameGraph): mFrameGraphToCreateDescriptors(frameGraph)
{
}

D3D12::FrameGraphDescriptorCreator::~FrameGraphDescriptorCreator()
{
}

UINT64 D3D12::FrameGraphDescriptorCreator::GetSrvUavDescriptorCountNeeded() const
{
	if(mFrameGraphToCreateDescriptors == nullptr)
	{
		//An optimization to not recreate descriptor heap and all descriptors in it
		//after creating the frame graph, if descriptor heap is already created
		return 10;
	}

	UINT64 result = 0;
	for(size_t i = 0; i < mFrameGraphToCreateDescriptors->mRenderPasses.size(); i++)
	{
		result += mFrameGraphToCreateDescriptors->mRenderPasses[i]->GetPassDescriptorCountNeeded();
	}

	return result;
}

void D3D12::FrameGraphDescriptorCreator::RecreateSrvUavDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, D3D12_GPU_DESCRIPTOR_HANDLE startDescriptorGpu, D3D12_GPU_DESCRIPTOR_HANDLE prevStartDescriptorGpu)
{
	if(mFrameGraphToCreateDescriptors == nullptr)
	{
		return;
	}

	UINT descriptorCount = GetSrvUavDescriptorCountNeeded();
	if(descriptorCount == 0)
	{
		return;
	}

	//Frame graph stores the copy of its descriptors on non-shader-visible heap
	device->CopyDescriptorsSimple(descriptorCount, mFrameGraphToCreateDescriptors->mSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), startDescriptorCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	for(size_t i = 0; i < mFrameGraphToCreateDescriptors->mRenderPasses.size(); i++)
	{
		//Frame graph passes will recalculate the new address for descriptors from the old ones
		mFrameGraphToCreateDescriptors->mRenderPasses[i]->ValidatePassDescriptors(prevStartDescriptorGpu, startDescriptorGpu);
	}
}