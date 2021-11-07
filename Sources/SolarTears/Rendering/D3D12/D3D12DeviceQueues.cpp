#include "D3D12DeviceQueues.hpp"
#include <cassert>
#include <vector>
#include "D3D12Utils.hpp"
#include <array>

D3D12::DeviceQueues::DeviceQueues(ID3D12Device* device)
{
	CreateQueueObjects(device);

	mGraphicsFenceValue = 0;
	mComputeFenceValue  = 0;
	mCopyFenceValue     = 0;

	THROW_IF_FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(mGraphicsFence.put())));
	THROW_IF_FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(mComputeFence.put())));
	THROW_IF_FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(mCopyFence.put())));

	mGraphicsFenceCompletionEvent = wil::unique_event(CreateEventEx(nullptr, L"Graphics fence wait", 0, EVENT_ALL_ACCESS));
	mComputeFenceCompletionEvent  = wil::unique_event(CreateEventEx(nullptr, L"Compute fence wait",  0, EVENT_ALL_ACCESS));
	mCopyFenceCompletionEvent     = wil::unique_event(CreateEventEx(nullptr, L"Copy fence wait",     0, EVENT_ALL_ACCESS));
}

ID3D12CommandQueue* D3D12::DeviceQueues::GraphicsQueueHandle() const
{
	return mGraphicsQueue.get();
}

ID3D12CommandQueue* D3D12::DeviceQueues::ComputeQueueHandle() const
{
	return mComputeQueue.get();
}

ID3D12CommandQueue* D3D12::DeviceQueues::CopyQueueHandle() const
{
	return mCopyQueue.get();
}

void D3D12::DeviceQueues::AllQueuesWaitStrong()
{
	THROW_IF_FAILED(mGraphicsQueue->Signal(mGraphicsFence.get(), ++mGraphicsFenceValue));
	THROW_IF_FAILED(mComputeQueue->Signal(mComputeFence.get(),   ++mComputeFenceValue));
	THROW_IF_FAILED(mCopyQueue->Signal(mCopyFence.get(),         ++mCopyFenceValue));

	THROW_IF_FAILED(mGraphicsFence->SetEventOnCompletion(mGraphicsFenceValue, mGraphicsFenceCompletionEvent.get()));
	THROW_IF_FAILED(mComputeFence->SetEventOnCompletion(mComputeFenceValue,   mComputeFenceCompletionEvent.get()));
	THROW_IF_FAILED(mCopyFence->SetEventOnCompletion(mCopyFenceValue,         mCopyFenceCompletionEvent.get()));

	std::array fenceEvents = {mGraphicsFenceCompletionEvent.get(), mComputeFenceCompletionEvent.get(), mCopyFenceCompletionEvent.get()};
	WaitForMultipleObjects((DWORD)fenceEvents.size(), fenceEvents.data(), TRUE, INFINITE);
}

UINT64 D3D12::DeviceQueues::GraphicsFenceSignal()
{
	THROW_IF_FAILED(mGraphicsQueue->Signal(mGraphicsFence.get(), ++mGraphicsFenceValue));
	return mGraphicsFenceValue;
}

UINT64 D3D12::DeviceQueues::ComputeFenceSignal()
{
	THROW_IF_FAILED(mComputeQueue->Signal(mComputeFence.get(), ++mComputeFenceValue));
	return mComputeFenceValue;
}

UINT64 D3D12::DeviceQueues::CopyFenceSignal()
{
	THROW_IF_FAILED(mCopyQueue->Signal(mCopyFence.get(), ++mCopyFenceValue));
	return mCopyFenceValue;
}

void D3D12::DeviceQueues::GraphicsQueueCpuWait(UINT64 fenceValue) const
{
	if(mGraphicsFence->GetCompletedValue() < fenceValue)
	{
		THROW_IF_FAILED(mGraphicsFence->SetEventOnCompletion(fenceValue, mGraphicsFenceCompletionEvent.get()));
		WaitForSingleObject(mGraphicsFenceCompletionEvent.get(), INFINITE);
	}
}

void D3D12::DeviceQueues::ComputeQueueCpuWait(UINT64 fenceValue) const
{
	if(mComputeFence->GetCompletedValue() < fenceValue)
	{
		THROW_IF_FAILED(mComputeFence->SetEventOnCompletion(fenceValue, mComputeFenceCompletionEvent.get()));
		WaitForSingleObject(mComputeFenceCompletionEvent.get(), INFINITE);
	}
}

void D3D12::DeviceQueues::CopyQueueCpuWait(UINT64 fenceValue) const
{
	if(mCopyFence->GetCompletedValue() < fenceValue)
	{
		THROW_IF_FAILED(mCopyFence->SetEventOnCompletion(fenceValue, mCopyFenceCompletionEvent.get()));
		WaitForSingleObject(mCopyFenceCompletionEvent.get(), INFINITE);
	}
}

void D3D12::DeviceQueues::GraphicsQueueWaitForCopyQueue(UINT64 fenceValue) const
{
	THROW_IF_FAILED(mGraphicsQueue->Wait(mCopyFence.get(), fenceValue));
}

void D3D12::DeviceQueues::CopyQueueWaitForGraphicsQueue(UINT64 fenceValue) const
{
	THROW_IF_FAILED(mCopyQueue->Wait(mGraphicsFence.get(), fenceValue));
}

void D3D12::DeviceQueues::GraphicsQueueWaitForComputeQueue(UINT64 fenceValue) const
{
	THROW_IF_FAILED(mGraphicsQueue->Wait(mComputeFence.get(), fenceValue));
}

void D3D12::DeviceQueues::ComputeQueueWaitForGraphicsQueue(UINT64 fenceValue) const
{
	THROW_IF_FAILED(mComputeQueue->Wait(mGraphicsFence.get(), fenceValue));
}

void D3D12::DeviceQueues::ComputeQueueWaitForCopyQueue(UINT64 fenceValue) const
{
	THROW_IF_FAILED(mComputeQueue->Wait(mCopyFence.get(), fenceValue));
}

void D3D12::DeviceQueues::CopyQueueWaitForComputeQueue(UINT64 fenceValue) const
{
	THROW_IF_FAILED(mCopyQueue->Wait(mComputeFence.get(), fenceValue));
}

void D3D12::DeviceQueues::CreateQueueObjects(ID3D12Device* device)
{
	//TODO: mGPU
	D3D12_COMMAND_QUEUE_DESC graphicsQueueDesc;
	graphicsQueueDesc.NodeMask = 0;
	graphicsQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	graphicsQueueDesc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
	graphicsQueueDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
	THROW_IF_FAILED(device->CreateCommandQueue(&graphicsQueueDesc, IID_PPV_ARGS(mGraphicsQueue.put())));

	//TODO: mGPU
	D3D12_COMMAND_QUEUE_DESC computeQueueDesc;
	computeQueueDesc.NodeMask = 0;
	computeQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	computeQueueDesc.Type     = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	computeQueueDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
	THROW_IF_FAILED(device->CreateCommandQueue(&computeQueueDesc, IID_PPV_ARGS(mComputeQueue.put())));

	//TODO: mGPU
	D3D12_COMMAND_QUEUE_DESC copyQueueDesc;
	copyQueueDesc.NodeMask = 0;
	copyQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	copyQueueDesc.Type     = D3D12_COMMAND_LIST_TYPE_COPY;
	copyQueueDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
	THROW_IF_FAILED(device->CreateCommandQueue(&copyQueueDesc, IID_PPV_ARGS(mCopyQueue.put())));
}