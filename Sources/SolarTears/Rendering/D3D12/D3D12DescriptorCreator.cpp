#include "D3D12DescriptorCreator.hpp"
#include "D3D12SrvDescriptorManager.hpp"

D3D12::DescriptorCreator::DescriptorCreator(FrameGraph* frameGraph, RenderableScene* renderableScene): mFrameGraphToCreateDescriptors(frameGraph), mSceneToMakeDescriptors(renderableScene)
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

	mSceneDescriptorsCount = 0;
	mPassDescriptorsCount  = 0;

	RequestDescriptors();
}

D3D12::DescriptorCreator::~DescriptorCreator()
{
}

UINT D3D12::DescriptorCreator::GetDescriptorCountNeeded()
{
	return mSceneDescriptorsCount + mPassDescriptorsCount;
}

void D3D12::DescriptorCreator::RecreateDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, D3D12_GPU_DESCRIPTOR_HANDLE startDescriptorGpu)
{
	UINT srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorAddress = startDescriptorCpu;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorAddress = startDescriptorGpu;
	RecreateSceneDescriptors(device, cpuDescriptorAddress, gpuDescriptorAddress, srvDescriptorSize);

	cpuDescriptorAddress.ptr += mSceneDescriptorsCount * srvDescriptorSize;
	gpuDescriptorAddress.ptr += mPassDescriptorsCount * srvDescriptorSize;
	RevalidatePassDescriptors(device, cpuDescriptorAddress, gpuDescriptorAddress);
}

void D3D12::DescriptorCreator::RequestDescriptors()
{
	mSceneDescriptorsCount = 0;
	mPassDescriptorsCount  = 0;

	for(size_t i = 0; i < mFrameGraphToCreateDescriptors->mRenderPasses.size(); i++)
	{
		mPassDescriptorsCount += mFrameGraphToCreateDescriptors->mRenderPasses[i]->GetPassDescriptorCountNeeded();
		mFrameGraphToCreateDescriptors->mRenderPasses[i]->RequestSceneDescriptors(this);
	}

	if(mTextureTableRequestState != DescriptorRequestState::NotRequested)
	{
		mSceneDescriptorsCount += (UINT)mSceneToMakeDescriptors->mSceneTextures.size();
	}

	if(mMaterialDataTableRequestState != DescriptorRequestState::NotRequested)
	{
		mSceneDescriptorsCount += (UINT)mSceneToMakeDescriptors->mMaterialDataSize / mSceneToMakeDescriptors->mMaterialChunkDataSize;
	}

	if(mStaticObjectDataTableRequestState != DescriptorRequestState::NotRequested)
	{
		mSceneDescriptorsCount += (UINT)mSceneToMakeDescriptors->mStaticObjectDataSize / mSceneToMakeDescriptors->mObjectChunkDataSize;
	}

	if(mFrameDataTableRequestState != DescriptorRequestState::NotRequested)
	{
		mSceneDescriptorsCount += Utils::InFlightFrameCount;
	}

	if(mDynamicObjectDataTableRequestState != DescriptorRequestState::NotRequested)
	{
		mSceneDescriptorsCount += (UINT)mSceneToMakeDescriptors->mCurrFrameDataToUpdate.size() * Utils::InFlightFrameCount;
	}
}

void D3D12::DescriptorCreator::RecreateSceneDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, D3D12_GPU_DESCRIPTOR_HANDLE startDescriptorGpu, UINT descriptorSize)
{
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

			cpuDescriptorAddress.ptr += descriptorSize;
			gpuDescriptorAddress.ptr += descriptorSize;
		}

		mTextureTableRequestState = DescriptorRequestState::Ready;
	}

	if(mMaterialDataTableRequestState == DescriptorRequestState::NotYetCreated)
	{
		uint32_t materialCount = (uint32_t)(mSceneToMakeDescriptors->mMaterialDataSize / mSceneToMakeDescriptors->mMaterialChunkDataSize);

		mMaterialDataDescriptorTableStart = gpuDescriptorAddress;
		for(uint32_t materialIndex = 0; materialIndex < materialCount; materialIndex++)
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = mSceneToMakeDescriptors->mSceneStaticConstantBuffer->GetGPUVirtualAddress() + mSceneToMakeDescriptors->CalculateMaterialDataOffset(materialIndex);
			cbvDesc.SizeInBytes    = sizeof(RenderableSceneMaterial);
		
			device->CreateConstantBufferView(&cbvDesc, cpuDescriptorAddress);

			cpuDescriptorAddress.ptr += descriptorSize;
			gpuDescriptorAddress.ptr += descriptorSize;
		}

		mMaterialDataTableRequestState = DescriptorRequestState::Ready;
	}

	if(mStaticObjectDataTableRequestState == DescriptorRequestState::NotYetCreated)
	{
		uint32_t staticObjectCount = (uint32_t)(mSceneToMakeDescriptors->mStaticObjectDataSize / mSceneToMakeDescriptors->mObjectChunkDataSize);

		mStaticObjectDataDescriptorTableStart = gpuDescriptorAddress;
		for(uint32_t objectDataIndex = 0; objectDataIndex < staticObjectCount; objectDataIndex++)
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = mSceneToMakeDescriptors->mSceneStaticConstantBuffer->GetGPUVirtualAddress() + mSceneToMakeDescriptors->CalculateStaticObjectDataOffset(objectDataIndex);
			cbvDesc.SizeInBytes    = sizeof(RenderableScene::PerObjectData);
		
			device->CreateConstantBufferView(&cbvDesc, cpuDescriptorAddress);

			cpuDescriptorAddress.ptr += descriptorSize;
			gpuDescriptorAddress.ptr += descriptorSize;
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

			cpuDescriptorAddress.ptr += descriptorSize;
			gpuDescriptorAddress.ptr += descriptorSize;
		}

		mFrameDataTableRequestState = DescriptorRequestState::Ready;
	}

	if(mDynamicObjectDataTableRequestState == DescriptorRequestState::NotYetCreated)
	{
		for(uint32_t frameIndex = 0; frameIndex < Utils::InFlightFrameCount; frameIndex++)
		{
			mDynamicObjectDataDescriptorTableStart[frameIndex] = gpuDescriptorAddress;
			for(uint32_t objectDataIndex = 0; objectDataIndex < mSceneToMakeDescriptors->mCurrFrameDataToUpdate.size(); objectDataIndex++)
			{
				D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
				cbvDesc.BufferLocation = mSceneToMakeDescriptors->mSceneDynamicConstantBuffer->GetGPUVirtualAddress() + mSceneToMakeDescriptors->CalculateRigidObjectDataOffset(frameIndex, objectDataIndex);
				cbvDesc.SizeInBytes    = sizeof(RenderableScene::PerObjectData);

				device->CreateConstantBufferView(&cbvDesc, cpuDescriptorAddress);

				cpuDescriptorAddress.ptr += descriptorSize;
				gpuDescriptorAddress.ptr += descriptorSize;
			}
		}

		mDynamicObjectDataTableRequestState = DescriptorRequestState::Ready;
	}
}

void D3D12::DescriptorCreator::RevalidatePassDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, D3D12_GPU_DESCRIPTOR_HANDLE startDescriptorGpu)
{
	//Frame graph stores the copy of its descriptors on non-shader-visible heap
	device->CopyDescriptorsSimple(mPassDescriptorsCount, mFrameGraphToCreateDescriptors->mSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), startDescriptorCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	for (size_t i = 0; i < mFrameGraphToCreateDescriptors->mRenderPasses.size(); i++)
	{
		//Frame graph passes will recalculate the new address for descriptors from the old ones
		mFrameGraphToCreateDescriptors->mRenderPasses[i]->ValidatePassDescriptors(mFrameGraphToCreateDescriptors->mFrameGraphDescriptorStart, startDescriptorGpu);

		mFrameGraphToCreateDescriptors->mRenderPasses[i]->ValidateSceneDescriptors(this);
	}

	mFrameGraphToCreateDescriptors->mFrameGraphDescriptorStart = startDescriptorGpu;
}

void D3D12::DescriptorCreator::RequestTexturesDescriptorTable()
{
	mTextureTableRequestState = DescriptorRequestState::NotYetCreated;
}

void D3D12::DescriptorCreator::RequestStaticObjectDataDescriptorTable()
{
	mStaticObjectDataTableRequestState = DescriptorRequestState::NotYetCreated;
}

void D3D12::DescriptorCreator::RequestDynamicObjectDataDescriptorTable()
{
	mDynamicObjectDataTableRequestState = DescriptorRequestState::NotYetCreated;
}

void D3D12::DescriptorCreator::RequestFrameDataDescriptorTable()
{
	mFrameDataTableRequestState = DescriptorRequestState::NotYetCreated;
}

void D3D12::DescriptorCreator::RequestMaterialDataDescriptorTable()
{
	mMaterialDataTableRequestState = DescriptorRequestState::NotYetCreated;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12::DescriptorCreator::GetTextureDescriptorTableStart() const
{
	assert(mTextureTableRequestState == DescriptorRequestState::Ready);
	return mTextureDescriptorTableStart;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12::DescriptorCreator::GetMaterialDataDescriptorTableStart() const
{
	assert(mMaterialDataTableRequestState == DescriptorRequestState::Ready);
	return mMaterialDataDescriptorTableStart;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12::DescriptorCreator::GetStaticObjectDataDescriptorTableStart() const
{
	assert(mStaticObjectDataTableRequestState == DescriptorRequestState::Ready);
	return mStaticObjectDataDescriptorTableStart;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12::DescriptorCreator::GetFrameDataDescriptorTableStart(uint32_t frameIndex) const
{
	assert(mFrameDataTableRequestState == DescriptorRequestState::Ready);
	assert(frameIndex < Utils::InFlightFrameCount);
	return mFrameDataDescriptorTableStart[frameIndex];
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12::DescriptorCreator::GetDynamicObjectDataDescriptorTableStart(uint32_t frameIndex) const
{
	assert(mDynamicObjectDataTableRequestState == DescriptorRequestState::Ready);
	assert(frameIndex < Utils::InFlightFrameCount);
	return mDynamicObjectDataDescriptorTableStart[frameIndex];
}