#include "D3D12SceneDescriptorCreator.hpp"
#include "..\..\Common\RenderingUtils.hpp"
#include "..\..\Vulkan\Scene\VulkanSceneDescriptorCreator.hpp"

D3D12::SceneDescriptorCreator::SceneDescriptorCreator(RenderableScene* renderableScene): mSceneToMakeDescriptors(renderableScene)
{
}

D3D12::SceneDescriptorCreator::~SceneDescriptorCreator()
{
}

UINT D3D12::SceneDescriptorCreator::GetDescriptorCountNeeded()
{
	return (UINT)mSceneToMakeDescriptors->mSceneTextures.size() + (UINT)mSceneToMakeDescriptors->mSceneMaterials.size() + (UINT)mSceneToMakeDescriptors->mRigidSceneObjects.size() * Utils::InFlightFrameCount;
}

void D3D12::SceneDescriptorCreator::RecreateSceneDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, D3D12_GPU_DESCRIPTOR_HANDLE startDescriptorGpu, UINT srvDescriptorSize)
{
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorAddress = startDescriptorCpu;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorAddress = startDescriptorGpu;
	
	mSceneToMakeDescriptors->mSceneTextureDescriptorsStart = gpuDescriptorAddress;
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

		cpuDescriptorAddress.ptr += srvDescriptorSize;
		gpuDescriptorAddress.ptr += srvDescriptorSize;
	}

	mSceneToMakeDescriptors->mSceneMaterialDescriptorsStart = gpuDescriptorAddress;
	for(size_t materialIndex = 0; materialIndex < mSceneToMakeDescriptors->mSceneMaterials.size(); materialIndex++)
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = mSceneToMakeDescriptors->mSceneConstantBuffer->GetGPUVirtualAddress() + mSceneToMakeDescriptors->CalculatePerMaterialDataOffset(materialIndex);
		cbvDesc.SizeInBytes    = sizeof(RenderableScene::SceneMaterial);
		
		device->CreateConstantBufferView(&cbvDesc, cpuDescriptorAddress);

		cpuDescriptorAddress.ptr += srvDescriptorSize;
		gpuDescriptorAddress.ptr += srvDescriptorSize;
	}

	for(uint32_t frameIndex = 0; frameIndex < Utils::InFlightFrameCount; frameIndex++)
	{
		mSceneToMakeDescriptors->mSceneObjectDescriptorsStart[frameIndex] = gpuDescriptorAddress;
		for(uint32_t objectIndex = 0; objectIndex < mSceneToMakeDescriptors->mRigidSceneObjects.size(); objectIndex++)
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = mSceneToMakeDescriptors->mSceneConstantBuffer->GetGPUVirtualAddress() + mSceneToMakeDescriptors->CalculatePerObjectDataOffset(frameIndex, objectIndex);
			cbvDesc.SizeInBytes    = sizeof(RenderableScene::PerObjectData);
		
			device->CreateConstantBufferView(&cbvDesc, cpuDescriptorAddress);

			cpuDescriptorAddress.ptr += srvDescriptorSize;
			gpuDescriptorAddress.ptr += srvDescriptorSize;
		}
	}
}