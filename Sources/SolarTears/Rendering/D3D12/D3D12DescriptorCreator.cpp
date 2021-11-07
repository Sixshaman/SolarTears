#include "D3D12DescriptorCreator.hpp"
#include "D3D12SrvDescriptorManager.hpp"

D3D12::DescriptorCreator::DescriptorCreator(FrameGraph* frameGraph, RenderableScene* renderableScene): mFrameGraphToCreateDescriptors(frameGraph), mSceneToMakeDescriptors(renderableScene)
{
	mTextureTableRequestState      = DescriptorRequestState::NotRequested;
	mMaterialDataTableRequestState = DescriptorRequestState::NotRequested;
	mFrameDataTableRequestState    = DescriptorRequestState::NotRequested;
	mObjectDataTableRequestState   = DescriptorRequestState::NotRequested;

	mTextureDescriptorTableStart      = {.ptr = (UINT64)(-1)};
	mMaterialDataDescriptorTableStart = {.ptr = (UINT64)(-1)};
	mFrameDataDescriptorTableStart    = {.ptr = (UINT64)(-1)};
	mObjectDataDescriptorTableStart   = {.ptr = (UINT64)(-1)};

	mSceneDescriptorsCount = 5; //Minimum 1 for each type
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

	if(mFrameGraphToCreateDescriptors != nullptr)
	{
		cpuDescriptorAddress.ptr += mSceneDescriptorsCount * srvDescriptorSize;
		gpuDescriptorAddress.ptr += mSceneDescriptorsCount * srvDescriptorSize;
		RevalidatePassDescriptors(device, cpuDescriptorAddress, gpuDescriptorAddress);
	}
}

void D3D12::DescriptorCreator::RequestDescriptors()
{
	mSceneDescriptorsCount = 0;
	mPassDescriptorsCount  = 0;

	if(mFrameGraphToCreateDescriptors != nullptr)
	{
		for(size_t i = 0; i < mFrameGraphToCreateDescriptors->mRenderPasses.size(); i++)
		{
			mPassDescriptorsCount += mFrameGraphToCreateDescriptors->mRenderPasses[i]->GetPassDescriptorCountNeeded();
			mFrameGraphToCreateDescriptors->mRenderPasses[i]->RequestSceneDescriptors(this);
		}
	}

	if(mTextureTableRequestState != DescriptorRequestState::NotRequested)
	{
		mSceneDescriptorsCount += std::max((UINT)mSceneToMakeDescriptors->mSceneTextures.size(), 1u);
	}

	if(mMaterialDataTableRequestState != DescriptorRequestState::NotRequested)
	{
		mSceneDescriptorsCount += std::max(mSceneToMakeDescriptors->GetMaterialCount(), 1u);
	}

	if(mFrameDataTableRequestState != DescriptorRequestState::NotRequested)
	{
		mSceneDescriptorsCount += 1;
	}

	if(mObjectDataTableRequestState != DescriptorRequestState::NotRequested)
	{
		uint32_t objectCount = mSceneToMakeDescriptors->GetStaticObjectCount() + mSceneToMakeDescriptors->GetRigidObjectCount();
		mSceneDescriptorsCount += std::max(objectCount, 1u);
	}

	mSceneDescriptorsCount = std::max(mSceneDescriptorsCount, 1u);
}

void D3D12::DescriptorCreator::RecreateSceneDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, D3D12_GPU_DESCRIPTOR_HANDLE startDescriptorGpu, UINT descriptorSize)
{
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorAddress = startDescriptorCpu;
	D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorAddress = startDescriptorGpu;
	
	if(mTextureTableRequestState == DescriptorRequestState::NotYetCreated)
	{
		mTextureDescriptorTableStart = gpuDescriptorAddress;
		if(mSceneToMakeDescriptors->mSceneTextures.size() == 0)
		{ 
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
			srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D; //Only 2D-textures are stored in mSceneTextures
			srvDesc.Format                        = DXGI_FORMAT_R8G8B8A8_UNORM;
			srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MostDetailedMip     = 0;
			srvDesc.Texture2D.MipLevels           = 1;
			srvDesc.Texture2D.PlaneSlice          = 0;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		
			device->CreateShaderResourceView(nullptr, nullptr, cpuDescriptorAddress);

			cpuDescriptorAddress.ptr += descriptorSize;
			gpuDescriptorAddress.ptr += descriptorSize;
		}
		else
		{
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
		}

		mTextureTableRequestState = DescriptorRequestState::Ready;
	}

	if(mMaterialDataTableRequestState == DescriptorRequestState::NotYetCreated)
	{
		uint32_t materialCount = mSceneToMakeDescriptors->GetMaterialCount();

		mMaterialDataDescriptorTableStart = gpuDescriptorAddress;
		if(materialCount == 0)
		{
			device->CreateConstantBufferView(nullptr, cpuDescriptorAddress);

			cpuDescriptorAddress.ptr += descriptorSize;
			gpuDescriptorAddress.ptr += descriptorSize;
		}
		else
		{
			for(uint32_t materialIndex = 0; materialIndex < materialCount; materialIndex++)
			{
				D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
				cbvDesc.BufferLocation = mSceneToMakeDescriptors->GetMaterialDataStart(materialIndex);
				cbvDesc.SizeInBytes    = mSceneToMakeDescriptors->mMaterialChunkDataSize;
		
				device->CreateConstantBufferView(&cbvDesc, cpuDescriptorAddress);

				cpuDescriptorAddress.ptr += descriptorSize;
				gpuDescriptorAddress.ptr += descriptorSize;
			}
		}

		mMaterialDataTableRequestState = DescriptorRequestState::Ready;
	}

	if(mFrameDataTableRequestState == DescriptorRequestState::NotYetCreated)
	{
		mFrameDataDescriptorTableStart = gpuDescriptorAddress;

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = mSceneToMakeDescriptors->GetFrameDataStart();
		cbvDesc.SizeInBytes    = mSceneToMakeDescriptors->mFrameChunkDataSize;
		
		device->CreateConstantBufferView(&cbvDesc, cpuDescriptorAddress);

		cpuDescriptorAddress.ptr += descriptorSize;
		gpuDescriptorAddress.ptr += descriptorSize;

		mFrameDataTableRequestState = DescriptorRequestState::Ready;
	}

	if(mObjectDataTableRequestState == DescriptorRequestState::NotYetCreated)
	{
		uint32_t objectCount = mSceneToMakeDescriptors->GetStaticObjectCount() + mSceneToMakeDescriptors->GetRigidObjectCount();

		mObjectDataDescriptorTableStart = gpuDescriptorAddress;
		if(objectCount == 0)
		{
			device->CreateConstantBufferView(nullptr, cpuDescriptorAddress);

			cpuDescriptorAddress.ptr += descriptorSize;
			gpuDescriptorAddress.ptr += descriptorSize;
		}
		else
		{
			for(uint32_t objectDataIndex = 0; objectDataIndex < objectCount; objectDataIndex++)
			{
				D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
				cbvDesc.BufferLocation = mSceneToMakeDescriptors->GetObjectDataStart(objectDataIndex);
				cbvDesc.SizeInBytes    = mSceneToMakeDescriptors->mObjectChunkDataSize;
		
				device->CreateConstantBufferView(&cbvDesc, cpuDescriptorAddress);

				cpuDescriptorAddress.ptr += descriptorSize;
				gpuDescriptorAddress.ptr += descriptorSize;
			}
		}

		mObjectDataTableRequestState = DescriptorRequestState::Ready;
	}
}

void D3D12::DescriptorCreator::RevalidatePassDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, D3D12_GPU_DESCRIPTOR_HANDLE startDescriptorGpu)
{
	//Frame graph stores the copy of its descriptors on non-shader-visible heap
	if (mPassDescriptorsCount != 0)
	{
		device->CopyDescriptorsSimple(mPassDescriptorsCount, mFrameGraphToCreateDescriptors->mSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), startDescriptorCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	for(size_t i = 0; i < mFrameGraphToCreateDescriptors->mRenderPasses.size(); i++)
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

void D3D12::DescriptorCreator::RequestMaterialDataDescriptorTable()
{
	mMaterialDataTableRequestState = DescriptorRequestState::NotYetCreated;
}

void D3D12::DescriptorCreator::RequestFrameDataDescriptorTable()
{
	mFrameDataTableRequestState = DescriptorRequestState::NotYetCreated;
}

void D3D12::DescriptorCreator::RequestObjectDataDescriptorTable()
{
	mObjectDataTableRequestState = DescriptorRequestState::NotYetCreated;
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

D3D12_GPU_DESCRIPTOR_HANDLE D3D12::DescriptorCreator::GetFrameDataDescriptorTableStart() const
{
	assert(mFrameDataTableRequestState == DescriptorRequestState::Ready);
	return mFrameDataDescriptorTableStart;
}

D3D12_GPU_DESCRIPTOR_HANDLE D3D12::DescriptorCreator::GetObjectDataDescriptorTableStart() const
{
	assert(mObjectDataTableRequestState == DescriptorRequestState::Ready);
	return mObjectDataDescriptorTableStart;
}