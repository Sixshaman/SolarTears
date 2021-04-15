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

	return 0;
}

UINT D3D12::FrameGraphDescriptorCreator::GetRtvDescriptorCountNeeded()
{
	return 0;
}

UINT D3D12::FrameGraphDescriptorCreator::GetDsvDescriptorCountNeeded()
{
	return 0;
}

void D3D12::FrameGraphDescriptorCreator::RecreateFrameGraphSrvUavDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, D3D12_GPU_DESCRIPTOR_HANDLE startDescriptorGpu, UINT srvDescriptorSize)
{
	UNREFERENCED_PARAMETER(device);
	UNREFERENCED_PARAMETER(startDescriptorCpu);
	UNREFERENCED_PARAMETER(startDescriptorGpu);
	UNREFERENCED_PARAMETER(srvDescriptorSize);
}

void D3D12::FrameGraphDescriptorCreator::RecreateFrameGraphRtvDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, UINT rtvDescriptorSize)
{
	UNREFERENCED_PARAMETER(device);
	UNREFERENCED_PARAMETER(startDescriptorCpu);
	UNREFERENCED_PARAMETER(rtvDescriptorSize);
}

void D3D12::FrameGraphDescriptorCreator::RecreateFrameGraphDsvDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, UINT dsvDescriptorSize)
{
	UNREFERENCED_PARAMETER(device);
	UNREFERENCED_PARAMETER(startDescriptorCpu);
	UNREFERENCED_PARAMETER(dsvDescriptorSize);
}