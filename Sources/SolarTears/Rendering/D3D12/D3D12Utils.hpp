#pragma once

#include <d3d12.h>
#include <string>
#include <vector>
#include <DirectXMath.h>
#include "../Common/RenderingUtils.hpp"

class LoggerQueue;

namespace D3D12
{
	namespace D3D12Utils
	{
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

		bool IsStateWriteable(D3D12_RESOURCE_STATES state);
		bool IsStateComputeFriendly(D3D12_RESOURCE_STATES state);
		bool IsStatePromoteableTo(D3D12_RESOURCE_STATES state);

		DXGI_FORMAT ConvertToTypeless(DXGI_FORMAT format);


		class StateSubobjectHelper
		{
		public:
			StateSubobjectHelper();
			~StateSubobjectHelper();

			template<typename T>
			void AddSubobjectGeneric(const T& subobject);

			void AddVertexShader(const void* shaderBlob, size_t shaderDataSize);
			void AddHullShader(const void* shaderBlob, size_t shaderDataSize);
			void AddDomainShader(const void* shaderBlob, size_t shaderDataSize);
			void AddGeometryShader(const void* shaderBlob, size_t shaderDataSize);
			void AddPixelShader(const void* shaderBlob, size_t shaderDataSize);

			void AddComputeShader(const void* shaderBlob, size_t shaderDataSize);

			void AddAmplificationShader(const void* shaderBlob, size_t shaderDataSize);
			void AddMeshShader(const void* shaderBlob, size_t shaderDataSize);

			void AddSampleMask(UINT sampleMask);
			void AddNodeMask(UINT nodeMask);
			void AddDepthStencilFormat(DXGI_FORMAT format);

			void*  GetStreamPointer() const;
			size_t GetStreamSize()    const;

		private:
			void AllocateStreamData(size_t data);

			void AddPadding();

			template<typename T>
			void AddStreamData(const T* data);

			void AddShader(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE subobjectType, const void* shaderData, size_t shaderSize);

			void ReallocateData(size_t requiredSize);

		private:
			std::byte* mSubobjectStreamBlob; //Have to control memory manually here... There's no properly aligned std::vector 
			size_t     mStreamBlobSize;
			size_t     mStreamBlobCapacity;
		};

		#include "D3D12Utils.inl"
	}
}