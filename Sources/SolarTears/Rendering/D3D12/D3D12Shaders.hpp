#pragma once

#include <d3d12.h>
#include <d3d12shader.h>
#include <dxc/dxcapi.h>
#include <unordered_map>
#include <string>
#include <array>
#include <wil/com.h>

class LoggerQueue;

namespace D3D12
{
	class ShaderManager
	{
		static constexpr uint32_t StaticSamplerCount = 2;

	public:
		ShaderManager(LoggerQueue* logger, ID3D12Device* device);
		~ShaderManager();

	private:
		void LoadShaderData(ID3D12Device* device);

		void CreateReflectionData(IDxcUtils* dxcUtils, IDxcBlobEncoding* pBlob, ID3D12ShaderReflection** outShaderReflection);

		void BuildGBufferRootSignature(ID3D12Device* device, ID3D12ShaderReflection* vsReflection, ID3D12ShaderReflection* psReflection);

	private:
		LoggerQueue* mLogger;

		wil::com_ptr_nothrow<ID3D12RootSignature> mGBufferRootSignature;
	};
}