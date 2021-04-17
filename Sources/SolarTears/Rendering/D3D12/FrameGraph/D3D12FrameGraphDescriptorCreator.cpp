#include "D3D12FrameGraphDescriptorCreator.hpp"

D3D12::FrameGraphDescriptorCreator::FrameGraphDescriptorCreator(FrameGraph* frameGraph): mFrameGraphToCreateDescriptors(frameGraph)
{
}

D3D12::FrameGraphDescriptorCreator::~FrameGraphDescriptorCreator()
{
}

UINT D3D12::FrameGraphDescriptorCreator::GetSrvUavDescriptorCountNeeded()
{
	if(mFrameGraphToCreateDescriptors == nullptr)
	{
		//An optimization to not recreate descriptor heap and all descriptors in it
		//after creating the frame graph, if descriptor heap is already created
		return 10;
	}

	UINT descriptorCountNeeded = 0;
	for(size_t i = 0; i < mFrameGraphToCreateDescriptors->mTextures.size(); i++)
	{
		if(i == mFrameGraphToCreateDescriptors->mBackbufferRefIndex)
		{
			//No srv/uav for the backbuffer
			continue;
		}

		D3D12_RESOURCE_DESC1 texDesc = mFrameGraphToCreateDescriptors->mTextures[i]->GetDesc1();
		if(((texDesc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) == 0) || (texDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
		{
			descriptorCountNeeded++;
		}
	}

	return descriptorCountNeeded;
}

UINT D3D12::FrameGraphDescriptorCreator::GetRtvDescriptorCountNeeded()
{
	if(mFrameGraphToCreateDescriptors == nullptr)
	{
		return 0;
	}

	UINT descriptorCountNeeded = 0;
	for(size_t i = 0; i < mFrameGraphToCreateDescriptors->mTextures.size(); i++)
	{
		if(i == mFrameGraphToCreateDescriptors->mBackbufferRefIndex)
		{
			continue; //Backbuffer is handled separately
		}

		D3D12_RESOURCE_DESC1 texDesc = mFrameGraphToCreateDescriptors->mTextures[i]->GetDesc1();
		if(texDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
		{
			descriptorCountNeeded++;
		}
	}

	for(size_t i = 0; mFrameGraphToCreateDescriptors->mSwapchainImageRefs.size(); i++)
	{
		D3D12_RESOURCE_DESC1 texDesc = mFrameGraphToCreateDescriptors->mTextures[i]->GetDesc1();
		if(texDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
		{
			descriptorCountNeeded++;
		}
	}

	return descriptorCountNeeded;
}

UINT D3D12::FrameGraphDescriptorCreator::GetDsvDescriptorCountNeeded()
{
	if(mFrameGraphToCreateDescriptors == nullptr)
	{
		return 0;
	}

	UINT descriptorCountNeeded = 0;
	for(size_t i = 0; i < mFrameGraphToCreateDescriptors->mTextures.size(); i++)
	{
		if(i == mFrameGraphToCreateDescriptors->mBackbufferRefIndex)
		{
			continue; //Backbuffer is handled separately
		}

		D3D12_RESOURCE_DESC1 texDesc = mFrameGraphToCreateDescriptors->mTextures[i]->GetDesc1();
		if(texDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
		{
			descriptorCountNeeded++;
		}
	}

	for(size_t i = 0; mFrameGraphToCreateDescriptors->mSwapchainImageRefs.size(); i++)
	{
		D3D12_RESOURCE_DESC1 texDesc = mFrameGraphToCreateDescriptors->mTextures[i]->GetDesc1();
		if(texDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
		{
			descriptorCountNeeded++;
		}
	}

	return descriptorCountNeeded;
}

void D3D12::FrameGraphDescriptorCreator::RecreateFrameGraphSrvUavDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, D3D12_GPU_DESCRIPTOR_HANDLE startDescriptorGpu, UINT srvDescriptorSize)
{
	UNREFERENCED_PARAMETER(device);
	UNREFERENCED_PARAMETER(startDescriptorCpu);
	UNREFERENCED_PARAMETER(startDescriptorGpu);
	UNREFERENCED_PARAMETER(srvDescriptorSize);

	//In case of TYPELESS textures:
	//The format will be resolved inside Render Pass object via a special function
}

void D3D12::FrameGraphDescriptorCreator::RecreateFrameGraphRtvDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, UINT rtvDescriptorSize)
{
	UNREFERENCED_PARAMETER(device);
	UNREFERENCED_PARAMETER(startDescriptorCpu);
	UNREFERENCED_PARAMETER(rtvDescriptorSize);

	//In case of TYPELESS textures:
	//The format will be resolved inside Render Pass object via a special function
}

void D3D12::FrameGraphDescriptorCreator::RecreateFrameGraphDsvDescriptors(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE startDescriptorCpu, UINT dsvDescriptorSize)
{
	UNREFERENCED_PARAMETER(device);
	UNREFERENCED_PARAMETER(startDescriptorCpu);
	UNREFERENCED_PARAMETER(dsvDescriptorSize);

	//In case of TYPELESS textures:
	//The format will be resolved inside Render Pass object via a special function
}