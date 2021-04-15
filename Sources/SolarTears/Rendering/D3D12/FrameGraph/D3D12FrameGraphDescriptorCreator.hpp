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
		UINT GetRtvDescriptorCountNeeded();
		UINT GetDsvDescriptorCountNeeded();

		//For the cases when there's need to recreate all descriptors (frame graph recreation, descriptor heap reallocation)
		//We could've copy descriptors in case of descriptor heap reallocation (without recreating them from scratch), but copying descriptors from shader-visible heaps is prohibitively slow
		void RecreateFrameGraphSrvUavDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, D3D12_GPU_DESCRIPTOR_HANDLE startDescriptorGpu, UINT srvDescriptorSize);
		void RecreateFrameGraphRtvDescriptors(ID3D12Device* device,    D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, UINT rtvDescriptorSize);
		void RecreateFrameGraphDsvDescriptors(ID3D12Device* device,    D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, UINT dsvDescriptorSize);

	private:
		FrameGraph* mFrameGraphToCreateDescriptors;
	};
}