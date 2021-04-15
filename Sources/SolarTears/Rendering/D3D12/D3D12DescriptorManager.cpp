#include "D3D12DescriptorManager.hpp"
#include "Scene/D3D12SceneDescriptorCreator.hpp"
#include "FrameGraph/D3D12FrameGraphDescriptorCreator.hpp"

D3D12::DescriptorManager::DescriptorManager(ID3D12Device* device): mSceneSrvDescriptorCount(0), mFrameGraphSrvDescriptorCount(0), mRtvDescriptorCount(0), mDsvDescriptorCount(0)
{
	mSrvUavCbvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	mRtvDescriptorSize       = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize       = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

D3D12::DescriptorManager::~DescriptorManager()
{
}

void D3D12::DescriptorManager::ValidateDescriptorHeaps(ID3D12Device* device, SceneDescriptorCreator* sceneDescriptorCreator, FrameGraphDescriptorCreator* frameGraphDescriptorCreator, uint32_t flags)
{
	//Set minimum values
	UINT sceneSrvDescriptorsCount      = sceneDescriptorCreator->GetDescriptorCountNeeded();
	UINT frameGraphSrvDescriptorsCount = frameGraphDescriptorCreator->GetSrvUavDescriptorCountNeeded();
	UINT rtvDescriptorsCount           = frameGraphDescriptorCreator->GetRtvDescriptorCountNeeded();
	UINT dsvDescriptorsCount           = frameGraphDescriptorCreator->GetDsvDescriptorCountNeeded();
	UINT srvDescriptorsCount           = sceneSrvDescriptorsCount + frameGraphSrvDescriptorsCount;

	if(srvDescriptorsCount > (mSceneSrvDescriptorCount + mFrameGraphSrvDescriptorCount))
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

		sceneDescriptorCreator->RecreateSceneDescriptors(device, srvHeapDescriptorAddressCpu, srvHeapDescriptorAddressGpu, mSrvUavCbvDescriptorSize);


		//Recreate frame graph descriptors
		srvHeapDescriptorAddressCpu.ptr += sceneSrvDescriptorsCount * mSrvUavCbvDescriptorSize;
		srvHeapDescriptorAddressGpu.ptr += sceneSrvDescriptorsCount * mSrvUavCbvDescriptorSize;

		frameGraphDescriptorCreator->RecreateFrameGraphSrvUavDescriptors(device, srvHeapDescriptorAddressCpu, srvHeapDescriptorAddressGpu, mSrvUavCbvDescriptorSize);

		mSrvUavCbvDescriptorHeap.swap(newSrvDescriptorHeap);
	}
	else if(srvDescriptorsCount > 0)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE srvHeapDescriptorAddressCpu = mSrvUavCbvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_GPU_DESCRIPTOR_HANDLE srvHeapDescriptorAddressGpu = mSrvUavCbvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

		if(!(flags & FLAG_SCENE_UNCHANGED))
		{
			sceneDescriptorCreator->RecreateSceneDescriptors(device, srvHeapDescriptorAddressCpu, srvHeapDescriptorAddressGpu, mSceneSrvDescriptorCount);
		}

		srvHeapDescriptorAddressCpu.ptr += sceneSrvDescriptorsCount * mSrvUavCbvDescriptorSize;
		srvHeapDescriptorAddressGpu.ptr += sceneSrvDescriptorsCount * mSrvUavCbvDescriptorSize;

		if(sceneSrvDescriptorsCount > mSceneSrvDescriptorCount || !(flags & FLAG_FRAME_GRAPH_UNCHANGED)) //Scene descriptors erased some of frame graph descriptors, or we need to rebuild the frame graph itself
		{
			frameGraphDescriptorCreator->RecreateFrameGraphSrvUavDescriptors(device, srvHeapDescriptorAddressCpu, srvHeapDescriptorAddressGpu, mSrvUavCbvDescriptorSize);
		}
	}

	mSceneSrvDescriptorCount      = sceneSrvDescriptorsCount;
	mFrameGraphSrvDescriptorCount = frameGraphSrvDescriptorsCount;


	//RTV descriptors
	if(rtvDescriptorsCount > mRtvDescriptorCount)
	{
		//TODO: mGPU?
		D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc;
		rtvDescriptorHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvDescriptorHeapDesc.NumDescriptors = rtvDescriptorsCount;
		rtvDescriptorHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		rtvDescriptorHeapDesc.NodeMask       = 0;

		wil::com_ptr_nothrow<ID3D12DescriptorHeap> newRtvDescriptorHeap;
		THROW_IF_FAILED(device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(newRtvDescriptorHeap.put())));


		//Recreate frame graph descriptors
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapDescriptorAddressCpu = newRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

		frameGraphDescriptorCreator->RecreateFrameGraphRtvDescriptors(device, rtvHeapDescriptorAddressCpu, mRtvDescriptorSize);

		mRtvDescriptorHeap.swap(newRtvDescriptorHeap);
	}
	else if (rtvDescriptorsCount > 0)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapDescriptorAddressCpu = mRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		if(!(flags & FLAG_FRAME_GRAPH_UNCHANGED))
		{
			frameGraphDescriptorCreator->RecreateFrameGraphRtvDescriptors(device, rtvHeapDescriptorAddressCpu, mRtvDescriptorSize);
		}
	}

	mRtvDescriptorCount = rtvDescriptorsCount;


	//DSV descriptors
	if(dsvDescriptorsCount > mDsvDescriptorCount)
	{
		//TODO: mGPU?
		D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc;
		dsvDescriptorHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvDescriptorHeapDesc.NumDescriptors = dsvDescriptorsCount;
		dsvDescriptorHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		dsvDescriptorHeapDesc.NodeMask       = 0;

		wil::com_ptr_nothrow<ID3D12DescriptorHeap> newDsvDescriptorHeap;
		THROW_IF_FAILED(device->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(newDsvDescriptorHeap.put())));


		//Recreate frame graph descriptors
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHeapDescriptorAddressCpu = newDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

		frameGraphDescriptorCreator->RecreateFrameGraphDsvDescriptors(device, dsvHeapDescriptorAddressCpu, mDsvDescriptorSize);

		mDsvDescriptorHeap.swap(newDsvDescriptorHeap);
	}
	else if (dsvDescriptorsCount > 0)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHeapDescriptorAddressCpu = mDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		if(!(flags & FLAG_FRAME_GRAPH_UNCHANGED))
		{
			frameGraphDescriptorCreator->RecreateFrameGraphDsvDescriptors(device, dsvHeapDescriptorAddressCpu, mDsvDescriptorSize);
		}
	}

	mDsvDescriptorCount = dsvDescriptorsCount;
}