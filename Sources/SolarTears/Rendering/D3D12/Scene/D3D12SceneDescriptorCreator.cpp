#include "D3D12SceneDescriptorCreator.hpp"
#include "..\..\Vulkan\Scene\VulkanSceneDescriptorCreator.hpp"

D3D12::SceneDescriptorCreator::SceneDescriptorCreator(RenderableScene* renderableScene): mSceneToMakeDescriptors(renderableScene)
{
}

D3D12::SceneDescriptorCreator::~SceneDescriptorCreator()
{
}

UINT D3D12::SceneDescriptorCreator::GetDescriptorCountNeeded()
{
	return (UINT)mSceneToMakeDescriptors->mSceneTextures.size();
}

void D3D12::SceneDescriptorCreator::RecreateSceneDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, D3D12_GPU_DESCRIPTOR_HANDLE startDescriptorGpu, UINT srvDescriptorSize)
{
	mSceneToMakeDescriptors->mSceneTextureDescriptors.clear();

	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorAddress = startDescriptorCpu;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorAddress = startDescriptorGpu;
	
	for(size_t i = 0; i < mSceneToMakeDescriptors->mSceneTextures.size(); i++)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D; //Only 2D-textures are stored in mSceneTextures
		srvDesc.Format                        = mSceneToMakeDescriptors->mSceneTextures[i]->GetDesc1().Format;
		srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MostDetailedMip     = 0;
		srvDesc.Texture2D.MipLevels           = (UINT)-1;
		srvDesc.Texture2D.PlaneSlice          = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		
		device->CreateShaderResourceView(mSceneToMakeDescriptors->mSceneTextures[i].get(), &srvDesc, cpuDescriptorAddress);
		mSceneToMakeDescriptors->mSceneTextureDescriptors.push_back(gpuDescriptorAddress);

		cpuDescriptorAddress.ptr += srvDescriptorSize;
		gpuDescriptorAddress.ptr += srvDescriptorSize;
	}
}