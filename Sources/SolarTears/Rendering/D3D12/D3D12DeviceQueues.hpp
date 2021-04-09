#pragma once

#include <d3d12.h>
#include <wil/com.h>

namespace D3D12
{
	class DeviceQueues
	{
	public:
		DeviceQueues(ID3D12Device* device);

		ID3D12CommandQueue* GraphicsQueueHandle() const;
		ID3D12CommandQueue* ComputeQueueHandle()  const;
		ID3D12CommandQueue* CopyQueueHandle()     const;

		void GraphicsQueueWait();
		void ComputeQueueWait();
		void CopyQueueWait();

		void AllQueuesWait();

	private:
		void CreateQueueObjects(ID3D12Device* device);

	private:
		wil::com_ptr_nothrow<ID3D12CommandQueue> mGraphicsQueue;
		wil::com_ptr_nothrow<ID3D12CommandQueue> mComputeQueue;
		wil::com_ptr_nothrow<ID3D12CommandQueue> mCopyQueue;

		wil::com_ptr_nothrow<ID3D12Fence1> mGraphicsFence;
		wil::com_ptr_nothrow<ID3D12Fence1> mComputeFence;
		wil::com_ptr_nothrow<ID3D12Fence1> mCopyFence;

		UINT64 mGraphicsFenceValue;
		UINT64 mComputeFenceValue;
		UINT64 mCopyFenceValue;

		wil::unique_event mGraphicsFenceCompletionEvent;
		wil::unique_event mComputeFenceCompletionEvent;
		wil::unique_event mCopyFenceCompletionEvent;
	};
}