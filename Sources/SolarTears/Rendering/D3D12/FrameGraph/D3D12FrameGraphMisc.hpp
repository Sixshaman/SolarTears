#pragma once

#include <d3d12.h>

namespace D3D12
{
	constexpr static UINT32 TextureFlagBarrierCommonPromoted = 0x01; //The barrier is automatically promoted from COMMON state

	//Additional D3D12-specific data for each subresource metadata node
	struct SubresourceMetadataPayload
	{
		SubresourceMetadataPayload()
		{
			Format = DXGI_FORMAT_UNKNOWN;
			State  = D3D12_RESOURCE_STATE_COMMON;
			Flags  = 0;
		}

		DXGI_FORMAT           Format;
		D3D12_RESOURCE_STATES State;
		UINT32                Flags;
	};
}