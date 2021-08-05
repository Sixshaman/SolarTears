#include "D3D12SceneDescriptorCreator.hpp"
#include "..\..\Common\RenderingUtils.hpp"
#include "..\..\Vulkan\Scene\VulkanSceneDescriptorCreator.hpp"

D3D12::SceneDescriptorCreator::SceneDescriptorCreator(RenderableScene* renderableScene): mSceneToMakeDescriptors(renderableScene)
{
	mTextureTableRequestState           = DescriptorRequestState::NotRequested;
	mStaticObjectDataTableRequestState  = DescriptorRequestState::NotRequested;
	mDynamicObjectDataTableRequestState = DescriptorRequestState::NotRequested;
	mFrameDataTableRequestState         = DescriptorRequestState::NotRequested;
	mMaterialDataTableRequestState      = DescriptorRequestState::NotRequested;

	mTextureDescriptorTableStart          = {.ptr = (UINT64)(-1)};
	mMaterialDataDescriptorTableStart     = {.ptr = (UINT64)(-1)};
	mStaticObjectDataDescriptorTableStart = {.ptr = (UINT64)(-1)};

	std::fill(std::begin(mFrameDataDescriptorTableStart),         std::end(mFrameDataDescriptorTableStart),         D3D12_GPU_DESCRIPTOR_HANDLE{.ptr = (UINT64)(-1)});
	std::fill(std::begin(mDynamicObjectDataDescriptorTableStart), std::end(mDynamicObjectDataDescriptorTableStart), D3D12_GPU_DESCRIPTOR_HANDLE{.ptr = (UINT64)(-1)});
}

D3D12::SceneDescriptorCreator::~SceneDescriptorCreator()
{
}

void D3D12::SceneDescriptorCreator::RequestTexturesDescriptorTable()
{
	mTextureTableRequestState = DescriptorRequestState::NotYetCreated;
}

void D3D12::SceneDescriptorCreator::RequestStaticObjectDataDescriptorTable()
{
	mStaticObjectDataTableRequestState = DescriptorRequestState::NotYetCreated;
}

void D3D12::SceneDescriptorCreator::RequestDynamicObjectDataDescriptorTable()
{
	mDynamicObjectDataTableRequestState = DescriptorRequestState::NotYetCreated;
}

void D3D12::SceneDescriptorCreator::RequestFrameDataDescriptorTable()
{
	mFrameDataTableRequestState = DescriptorRequestState::NotYetCreated;
}

void D3D12::SceneDescriptorCreator::RequestMaterialDataDescriptorTable()
{
	mMaterialDataTableRequestState = DescriptorRequestState::NotYetCreated;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12::SceneDescriptorCreator::GetTextureDescriptorTableStart() const
{
	assert(mTextureTableRequestState == DescriptorRequestState::Ready);
	return mTextureDescriptorTableStart;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12::SceneDescriptorCreator::GetMaterialDataDescriptorTableStart() const
{
	assert(mMaterialDataTableRequestState == DescriptorRequestState::Ready);
	return mMaterialDataDescriptorTableStart;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12::SceneDescriptorCreator::GetStaticObjectDataDescriptorTableStart() const
{
	assert(mStaticObjectDataTableRequestState == DescriptorRequestState::Ready);
	return mStaticObjectDataDescriptorTableStart;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12::SceneDescriptorCreator::GetFrameDataDescriptorTableStart(uint32_t frameIndex) const
{
	assert(mFrameDataTableRequestState == DescriptorRequestState::Ready);
	assert(frameIndex < Utils::InFlightFrameCount);
	return mFrameDataDescriptorTableStart[frameIndex];
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12::SceneDescriptorCreator::GetDynamicObjectDataDescriptorTableStart(uint32_t frameIndex) const
{
	assert(mDynamicObjectDataTableRequestState == DescriptorRequestState::Ready);
	assert(frameIndex < Utils::InFlightFrameCount);
	return mDynamicObjectDataDescriptorTableStart[frameIndex];
}

UINT64 D3D12::SceneDescriptorCreator::GetDescriptorCountNeeded() const
{
	UINT64 result = 0;

	if(mTextureTableRequestState != DescriptorRequestState::NotRequested)
	{
		result += mSceneToMakeDescriptors->mSceneTextures.size();
	}

	if(mMaterialDataTableRequestState != DescriptorRequestState::NotRequested)
	{
		result += mSceneToMakeDescriptors->mMaterialDataSize / mSceneToMakeDescriptors->mMaterialChunkDataSize;
	}

	if(mStaticObjectDataTableRequestState != DescriptorRequestState::NotRequested)
	{
		result += mSceneToMakeDescriptors->mStaticObjectDataSize / mSceneToMakeDescriptors->mObjectChunkDataSize;
	}

	if(mFrameDataTableRequestState != DescriptorRequestState::NotRequested)
	{
		result += Utils::InFlightFrameCount;
	}

	if(mDynamicObjectDataTableRequestState != DescriptorRequestState::NotRequested)
	{
		result += mSceneToMakeDescriptors->mCurrFrameDataToUpdate.size() * Utils::InFlightFrameCount;
	}

	return result;
}

void D3D12::SceneDescriptorCreator::RecreateDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, D3D12_GPU_DESCRIPTOR_HANDLE startDescriptorGpu)
{
	UINT srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorAddress = startDescriptorCpu;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorAddress = startDescriptorGpu;
	
	if(mTextureTableRequestState == DescriptorRequestState::NotYetCreated)
	{
		mTextureDescriptorTableStart = gpuDescriptorAddress;
		for(size_t textureIndex = 0; textureIndex < mSceneToMakeDescriptors->mSceneTextures.size(); textureIndex++)
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
			srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D; //Only 2D-textures are stored in mSceneTextures
			srvDesc.Format                        = mSceneToMakeDescriptors->mSceneTextures[textureIndex]->GetDesc1().Format;
			srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MostDetailedMip     = 0;
			srvDesc.Texture2D.MipLevels           = (UINT)-1;
			srvDesc.Texture2D.PlaneSlice          = 0;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		
			device->CreateShaderResourceView(mSceneToMakeDescriptors->mSceneTextures[textureIndex].get(), &srvDesc, cpuDescriptorAddress);

			cpuDescriptorAddress.ptr += srvDescriptorSize;
			gpuDescriptorAddress.ptr += srvDescriptorSize;
		}

		mTextureTableRequestState = DescriptorRequestState::Ready;
	}

	if(mMaterialDataTableRequestState == DescriptorRequestState::NotYetCreated)
	{
		uint32_t materialCount = mSceneToMakeDescriptors->mMaterialDataSize / mSceneToMakeDescriptors->mMaterialChunkDataSize;

		mMaterialDataDescriptorTableStart = gpuDescriptorAddress;
		for(uint32_t materialIndex = 0; materialIndex < materialCount; materialIndex++)
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = mSceneToMakeDescriptors->mSceneStaticConstantBuffer->GetGPUVirtualAddress() + mSceneToMakeDescriptors->CalculateMaterialDataOffset(materialIndex);
			cbvDesc.SizeInBytes    = sizeof(RenderableSceneMaterial);
		
			device->CreateConstantBufferView(&cbvDesc, cpuDescriptorAddress);

			cpuDescriptorAddress.ptr += srvDescriptorSize;
			gpuDescriptorAddress.ptr += srvDescriptorSize;
		}

		mMaterialDataTableRequestState = DescriptorRequestState::Ready;
	}

	if(mStaticObjectDataTableRequestState == DescriptorRequestState::NotYetCreated)
	{
		uint32_t staticObjectCount = mSceneToMakeDescriptors->mStaticObjectDataSize / mSceneToMakeDescriptors->mObjectChunkDataSize;

		mStaticObjectDataDescriptorTableStart = gpuDescriptorAddress;
		for(uint32_t objectDataIndex = 0; objectDataIndex < staticObjectCount; objectDataIndex++)
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = mSceneToMakeDescriptors->mSceneStaticConstantBuffer->GetGPUVirtualAddress() + mSceneToMakeDescriptors->CalculateStaticObjectDataOffset(objectDataIndex);
			cbvDesc.SizeInBytes    = sizeof(RenderableScene::PerObjectData);
		
			device->CreateConstantBufferView(&cbvDesc, cpuDescriptorAddress);

			cpuDescriptorAddress.ptr += srvDescriptorSize;
			gpuDescriptorAddress.ptr += srvDescriptorSize;
		}

		mStaticObjectDataTableRequestState = DescriptorRequestState::Ready;
	}

	if(mFrameDataTableRequestState == DescriptorRequestState::NotYetCreated)
	{
		for(uint32_t frameIndex = 0; frameIndex < Utils::InFlightFrameCount; frameIndex++)
		{
			mFrameDataDescriptorTableStart[frameIndex] = gpuDescriptorAddress;

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = mSceneToMakeDescriptors->mSceneDynamicConstantBuffer->GetGPUVirtualAddress() + mSceneToMakeDescriptors->CalculateFrameDataOffset(frameIndex);
			cbvDesc.SizeInBytes    = sizeof(RenderableScene::PerFrameData);
		
			device->CreateConstantBufferView(&cbvDesc, cpuDescriptorAddress);

			cpuDescriptorAddress.ptr += srvDescriptorSize;
			gpuDescriptorAddress.ptr += srvDescriptorSize;
		}

		mFrameDataTableRequestState = DescriptorRequestState::Ready;
	}

	if(mDynamicObjectDataTableRequestState == DescriptorRequestState::NotYetCreated)
	{
		for(uint32_t frameIndex = 0; frameIndex < Utils::InFlightFrameCount; frameIndex++)
		{
			mDynamicObjectDataDescriptorTableStart[frameIndex] = gpuDescriptorAddress;
			for(size_t objectDataIndex = 0; objectDataIndex < mSceneToMakeDescriptors->mCurrFrameDataToUpdate.size(); objectDataIndex++)
			{
				D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
				cbvDesc.BufferLocation = mSceneToMakeDescriptors->mSceneDynamicConstantBuffer->GetGPUVirtualAddress() + mSceneToMakeDescriptors->CalculateRigidObjectDataOffset(frameIndex, objectDataIndex);
				cbvDesc.SizeInBytes    = sizeof(RenderableScene::PerObjectData);

				device->CreateConstantBufferView(&cbvDesc, cpuDescriptorAddress);

				cpuDescriptorAddress.ptr += srvDescriptorSize;
				gpuDescriptorAddress.ptr += srvDescriptorSize;
			}
		}

		mDynamicObjectDataTableRequestState = DescriptorRequestState::Ready;
	}
}