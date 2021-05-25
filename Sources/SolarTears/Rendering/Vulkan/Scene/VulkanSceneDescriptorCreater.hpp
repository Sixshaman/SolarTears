#pragma once

#include "VulkanScene.hpp"

namespace Vulkan
{
	class SceneDescriptorCreator
	{
	public:
		SceneDescriptorCreator(RenderableScene* renderableScene);
		~SceneDescriptorCreator();

		UINT GetDescriptorCountNeeded();

		//Assumes the descriptor heap is big enough
		//We could've copy descriptors in case of descriptor heap reallocation (without recreating them from scratch), but copying descriptors from shader-visible heaps is prohibitively slow
		void RecreateSceneDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, D3D12_GPU_DESCRIPTOR_HANDLE startDescriptorGpu, UINT srvDescriptorSize);

	private:
		RenderableScene* mSceneToMakeDescriptors;
	};
}