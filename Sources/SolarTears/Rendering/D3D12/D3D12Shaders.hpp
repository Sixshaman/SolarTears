#pragma once

#include <d3d12.h>
#include <d3d12shader.h>
#include <dxc/dxcapi.h>
#include <unordered_map>
#include <string>
#include <array>
#include <wil/com.h>
#include <span>

class LoggerQueue;

namespace D3D12
{
	class ShaderManager
	{
		static constexpr uint32_t StaticSamplerCount = 2;

	public:
		//TODO: bindless (would eliminate the need for separate tables for textures, materials and object data)
		constexpr static UINT GBufferMaterialIndexBinding   = 0;
		constexpr static UINT GBufferObjectIndexBinding     = 1;
		constexpr static UINT GBufferPerObjectBufferBinding = 2;
		constexpr static UINT GBufferTextureBinding         = 3;
		constexpr static UINT GBufferMaterialBinding        = 4;
		constexpr static UINT GBufferPerFrameBufferBinding  = 5;


	public:
		ShaderManager(LoggerQueue* logger, ID3D12Device* device);
		~ShaderManager();

	public:
		const void* GetGBufferVertexShaderData() const;
		const void* GetGBufferPixelShaderData()  const;

		size_t GetGBufferVertexShaderSize()   const;
		size_t GetGBufferPixelShaderSize() const;

		ID3D12RootSignature* GetGBufferRootSignature() const;

	private:
		void BuildGBufferRootSignature(ID3D12Device* device, ID3D12ShaderReflection* vsReflection, ID3D12ShaderReflection* psReflection);

	private:
		void LoadShaderData(ID3D12Device* device);

		void CreateReflectionData(IDxcUtils* dxcUtils, IDxcBlobEncoding* pBlob, ID3D12ShaderReflection** outShaderReflection) const;

		void CollectBindingInfos(ID3D12ShaderReflection* reflection, std::unordered_map<std::string, D3D12_SHADER_INPUT_BIND_DESC>& outBindingInfos, std::unordered_map<std::string, D3D12_SHADER_VISIBILITY>& outShaderVisibility, D3D12_ROOT_SIGNATURE_FLAGS* rootSignatureFlags) const;

		void CreateRootSignature(ID3D12Device* device, const std::unordered_map<std::string, D3D12_SHADER_INPUT_BIND_DESC>& bindingInfos, const std::unordered_map<std::string, D3D12_SHADER_VISIBILITY>& visibilities, std::span<std::string_view> shaderInputNames, std::string_view samplersInputName, std::span<D3D12_ROOT_PARAMETER_TYPE> shaderInputTypes, D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags, ID3D12RootSignature** outRootSignature) const;

		D3D12_ROOT_SIGNATURE_FLAGS CreateDefaultRootSignatureFlags() const;

		D3D12_ROOT_CONSTANTS    CreateRootConstants(const D3D12_SHADER_INPUT_BIND_DESC& inputBindDesc, UINT numConstants) const;
		D3D12_ROOT_DESCRIPTOR1  CreateRootDescriptor(const D3D12_SHADER_INPUT_BIND_DESC& inputBindDesc, D3D12_ROOT_DESCRIPTOR_FLAGS flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE) const;
		D3D12_DESCRIPTOR_RANGE1 CreateRootDescriptorRange(const D3D12_SHADER_INPUT_BIND_DESC& inputBindDesc, UINT offsetFromTableStart, D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE) const;

	private:
		LoggerQueue* mLogger;

		wil::com_ptr_nothrow<ID3D12RootSignature> mGBufferRootSignature;

		wil::com_ptr_nothrow<IDxcBlobEncoding> mGBufferVertexShaderBlob;
		wil::com_ptr_nothrow<IDxcBlobEncoding> mGBufferPixelShaderBlob;
	};
}