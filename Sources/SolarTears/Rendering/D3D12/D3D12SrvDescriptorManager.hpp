#pragma once

#include <d3d12.h>
#include <wil/com.h>

namespace D3D12
{
	class SrvDescriptorManager
	{
	public:
		SrvDescriptorManager();
		~SrvDescriptorManager();

		ID3D12DescriptorHeap* GetDescriptorHeap() const;

		void ValidateDescriptorHeaps(ID3D12Device* device, UINT descriptorCountNeeded);

	private:
		wil::com_ptr_nothrow<ID3D12DescriptorHeap> mSrvUavCbvDescriptorHeap;
	};
}