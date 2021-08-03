#pragma once

#include "D3D12Scene.hpp"

namespace D3D12
{
	class SceneDescriptorCreator
	{
		enum class DescriptorRequestState: uint8_t
		{
			NotRequested,
			NotYetCreated,
			Ready
		};

	public:
		SceneDescriptorCreator(RenderableScene* renderableScene);
		~SceneDescriptorCreator();

	public:
		//Request the creation of the descriprors for the data type
		void RequestTexturesDescriptorTable();
		void RequestObjectDataDescriptorTable();
		void RequestFrameDataDescriptorTable();
		void RequestMaterialDataDescriptorTable();

		//Get the initialized descriptor table start for the data type
		D3D12_GPU_DESCRIPTOR_HANDLE GetTextureDescriptorTableStart()      const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetObjectDataDescriptorTableStart()   const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetFrameDataDescriptorTableStart()    const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetMaterialDataDescriptorTableStart() const;

		//Gets the necessary descriptor count needed for all requested descriptors
		UINT64 GetDescriptorCountNeeded();

		//Assumes the descriptor heap is big enough
		//We could've copy descriptors in case of descriptor heap reallocation (without recreating them from scratch), but copying descriptors from shader-visible heaps is prohibitively slow
		void RecreateDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, D3D12_GPU_DESCRIPTOR_HANDLE startDescriptorGpu);

	private:
		RenderableScene* mSceneToMakeDescriptors;

		DescriptorRequestState mTextureTableRequestState;
		DescriptorRequestState mObjectDataTableRequestState;
		DescriptorRequestState mFrameDataTableRequestState;
		DescriptorRequestState mMaterialDataTableRequestState;

		D3D12_GPU_DESCRIPTOR_HANDLE mTextureDescriptorTableStart;
		D3D12_GPU_DESCRIPTOR_HANDLE mObjectDataDescriptorTableStart;
		D3D12_GPU_DESCRIPTOR_HANDLE mFrameDataDescriptorTableStart;
		D3D12_GPU_DESCRIPTOR_HANDLE mMaterialDataDescriptorTableStart;
	};
}