#pragma once

#include <d3d12.h>
#include "FrameGraph/D3D12FrameGraph.hpp"
#include "Scene/D3D12Scene.hpp"

namespace D3D12
{
	class SrvDescriptorManager;

	class DescriptorCreator
	{
		enum class DescriptorRequestState: uint8_t
		{
			NotRequested,
			NotYetCreated,
			Ready
		};

	public:
		DescriptorCreator(FrameGraph* frameGraph, RenderableScene* renderableScene);
		~DescriptorCreator();

		UINT GetDescriptorCountNeeded();

		//Request the creation of the descriprors for the data type
		void RequestTexturesDescriptorTable();
		void RequestMaterialDataDescriptorTable();
		void RequestFrameDataDescriptorTable();
		void RequestObjectDataDescriptorTable();

		//Get the initialized descriptor table start for the data type
		D3D12_GPU_DESCRIPTOR_HANDLE GetTextureDescriptorTableStart()      const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetMaterialDataDescriptorTableStart() const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetFrameDataDescriptorTableStart()    const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetObjectDataDescriptorTableStart()   const;

		//Assumes the descriptor heap is big enough
		//For the cases when there's need to recreate all descriptors (frame graph recreation, scene descriptor heap reallocation)
		void RecreateDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, D3D12_GPU_DESCRIPTOR_HANDLE startDescriptorGpu);

	private:
		void RequestDescriptors();

		void RecreateSceneDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, D3D12_GPU_DESCRIPTOR_HANDLE startDescriptorGpu, UINT descriptorSize);
		void RevalidatePassDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, D3D12_GPU_DESCRIPTOR_HANDLE startDescriptorGpu);

	private:
		FrameGraph*      mFrameGraphToCreateDescriptors;
		RenderableScene* mSceneToMakeDescriptors;

		UINT mSceneDescriptorsCount;
		UINT mPassDescriptorsCount;

		//Requested scene descriptors
		DescriptorRequestState mTextureTableRequestState;
		DescriptorRequestState mMaterialDataTableRequestState;
		DescriptorRequestState mFrameDataTableRequestState;
		DescriptorRequestState mObjectDataTableRequestState;

		D3D12_GPU_DESCRIPTOR_HANDLE mTextureDescriptorTableStart;
		D3D12_GPU_DESCRIPTOR_HANDLE mMaterialDataDescriptorTableStart;
		D3D12_GPU_DESCRIPTOR_HANDLE mFrameDataDescriptorTableStart;
		D3D12_GPU_DESCRIPTOR_HANDLE mObjectDataDescriptorTableStart;
	};
}