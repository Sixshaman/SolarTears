#pragma once

#include <d3d12.h>
#include "D3D12FrameGraph.hpp"

namespace D3D12
{
	class FrameGraphDescriptorCreator
	{
	public:
		FrameGraphDescriptorCreator(FrameGraph* frameGraph);
		~FrameGraphDescriptorCreator();

		UINT GetSrvUavDescriptorCountNeeded();

		//For the cases when there's need to recreate all descriptors (frame graph recreation, descriptor heap reallocation)
		void RecreateFrameGraphSrvUavDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, D3D12_GPU_DESCRIPTOR_HANDLE startDescriptorGpu, D3D12_GPU_DESCRIPTOR_HANDLE prevStartDescriptorGpu);

	private:
		FrameGraph* mFrameGraphToCreateDescriptors;
	};
}