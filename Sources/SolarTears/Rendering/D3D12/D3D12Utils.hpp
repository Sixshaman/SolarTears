#pragma once

#include <d3d12.h>
#include <string>
#include <vector>
#include "../../../3rd party/DirectXMath/Inc/DirectXMath.h"

class LoggerQueue;

namespace D3D12
{
	namespace D3D12Utils
	{
		//3 frames in flight is good enough
		constexpr uint32_t InFlightFrameCount = 3;

		template<typename T>
		static constexpr DXGI_FORMAT FormatForVectorType = DXGI_FORMAT_UNKNOWN;

		template<>
		static constexpr DXGI_FORMAT FormatForVectorType<DirectX::XMFLOAT4> = DXGI_FORMAT_R32G32B32A32_FLOAT;

		template<>
		static constexpr DXGI_FORMAT FormatForVectorType<DirectX::XMFLOAT3> = DXGI_FORMAT_R32G32B32_FLOAT;

		template<>
		static constexpr DXGI_FORMAT FormatForVectorType<DirectX::XMFLOAT2> = DXGI_FORMAT_R32G32_FLOAT;

		template<typename T>
		static constexpr DXGI_FORMAT FormatForIndexType = DXGI_FORMAT_UNKNOWN;

		template<>
		static constexpr DXGI_FORMAT FormatForIndexType<uint32_t> = DXGI_FORMAT_R32_UINT;

		template<>
		static constexpr DXGI_FORMAT FormatForIndexType<uint16_t> = DXGI_FORMAT_R16_UINT;

		template<>
		static constexpr DXGI_FORMAT FormatForIndexType<uint8_t> = DXGI_FORMAT_R8_UINT;


		UINT64 AlignMemory(UINT64 value, UINT64 alignment);
	}
}