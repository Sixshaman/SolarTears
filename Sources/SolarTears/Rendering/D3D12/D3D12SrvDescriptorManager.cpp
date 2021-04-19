#include "D3D12SrvDescriptorManager.hpp"
#include "Scene/D3D12SceneDescriptorCreator.hpp"
#include "FrameGraph/D3D12FrameGraphDescriptorCreator.hpp"

D3D12::SrvDescriptorManager::SrvDescriptorManager(ID3D12Device* device): mSceneSrvDescriptorCount(0), mFrameGraphSrvDescriptorCount(0)
{
}

D3D12::SrvDescriptorManager::~SrvDescriptorManager()
{
}

void D3D12::SrvDescriptorManager::ValidateDescriptorHeaps(ID3D12Device* device, SceneDescriptorCreator* sceneDescriptorCreator, FrameGraphDescriptorCreator* frameGraphDescriptorCreator, uint32_t flags)
{
	UINT srvUavCbvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//Set minimum values
	UINT sceneSrvDescriptorsCount      = sceneDescriptorCreator->GetDescriptorCountNeeded();
	UINT frameGraphSrvDescriptorsCount = frameGraphDescriptorCreator->GetSrvUavDescriptorCountNeeded();
	UINT srvDescriptorsCount           = sceneSrvDescriptorsCount + frameGraphSrvDescriptorsCount;

	if(srvDescriptorsCount > (mSceneSrvDescriptorCount + mFrameGraphSrvDescriptorCount)) //Need to recreate the whole descriptor heap
	{
		//TODO: mGPU?
		D3D12_DESCRIPTOR_HEAP_DESC srvDescriptorHeapDesc;
		srvDescriptorHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvDescriptorHeapDesc.NumDescriptors = srvDescriptorsCount;
		srvDescriptorHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		srvDescriptorHeapDesc.NodeMask       = 0;

		wil::com_ptr_nothrow<ID3D12DescriptorHeap> newSrvDescriptorHeap;
		THROW_IF_FAILED(device->CreateDescriptorHeap(&srvDescriptorHeapDesc, IID_PPV_ARGS(newSrvDescriptorHeap.put())));


		//Recreate scene descriptors
		D3D12_CPU_DESCRIPTOR_HANDLE srvHeapDescriptorAddressCpu = newSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE srvHeapDescriptorAddressGpu = newSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

		sceneDescriptorCreator->RecreateSceneDescriptors(device, srvHeapDescriptorAddressCpu, srvHeapDescriptorAddressGpu, srvUavCbvDescriptorSize);


		//Recreate frame graph descriptors
		srvHeapDescriptorAddressCpu.ptr += sceneSrvDescriptorsCount * srvUavCbvDescriptorSize;
		srvHeapDescriptorAddressGpu.ptr += sceneSrvDescriptorsCount * srvUavCbvDescriptorSize;

		D3D12_GPU_DESCRIPTOR_HANDLE prevFrameGraphStartAddress;
		if(!(flags & FLAG_FRAME_GRAPH_UNCHANGED))
		{
			prevFrameGraphStartAddress.ptr = 0; //Frame graph was just created, the previous descriptors are laid in pseudo-heap starting from address 0
		}
		else
		{
			prevFrameGraphStartAddress.ptr = mSrvUavCbvDescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr + mSceneSrvDescriptorCount * srvUavCbvDescriptorSize;
		}

		frameGraphDescriptorCreator->RecreateFrameGraphSrvUavDescriptors(device, srvHeapDescriptorAddressCpu, srvHeapDescriptorAddressGpu, prevFrameGraphStartAddress);

		mSrvUavCbvDescriptorHeap.swap(newSrvDescriptorHeap);
	}
	else if(srvDescriptorsCount > 0) //Just need to rebind the descriptors
	{
		D3D12_CPU_DESCRIPTOR_HANDLE srvHeapDescriptorAddressCpu = mSrvUavCbvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE srvHeapDescriptorAddressGpu = mSrvUavCbvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

		if(!(flags & FLAG_SCENE_UNCHANGED))
		{
			sceneDescriptorCreator->RecreateSceneDescriptors(device, srvHeapDescriptorAddressCpu, srvHeapDescriptorAddressGpu, mSceneSrvDescriptorCount);
		}

		srvHeapDescriptorAddressCpu.ptr += sceneSrvDescriptorsCount * srvUavCbvDescriptorSize;
		srvHeapDescriptorAddressGpu.ptr += sceneSrvDescriptorsCount * srvUavCbvDescriptorSize;

		if(sceneSrvDescriptorsCount > mSceneSrvDescriptorCount || !(flags & FLAG_FRAME_GRAPH_UNCHANGED)) //Scene descriptors erased some of frame graph descriptors, or we need to rebuild the frame graph itself
		{
			D3D12_GPU_DESCRIPTOR_HANDLE prevFrameGraphStartAddress;
			if(!(flags & FLAG_FRAME_GRAPH_UNCHANGED))
			{
				prevFrameGraphStartAddress.ptr = 0; //Frame graph was just created, the previous descriptors are laid in pseudo-heap starting from address 0
			}
			else
			{
				prevFrameGraphStartAddress.ptr = mSrvUavCbvDescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr + mSceneSrvDescriptorCount * srvUavCbvDescriptorSize;
			}

			frameGraphDescriptorCreator->RecreateFrameGraphSrvUavDescriptors(device, srvHeapDescriptorAddressCpu, srvHeapDescriptorAddressGpu, prevFrameGraphStartAddress);
		}
	}

	mSceneSrvDescriptorCount      = sceneSrvDescriptorsCount;
	mFrameGraphSrvDescriptorCount = frameGraphSrvDescriptorsCount;
}