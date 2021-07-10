#include "D3D12Shaders.hpp"
#include "D3D12Utils.hpp"
#include "../../Core/Util.hpp"
#include "../../Logging/Logger.hpp"
#include <cassert>
#include <array>
#include <wil/com.h>

D3D12::ShaderManager::ShaderManager(LoggerQueue* logger, ID3D12Device* device): mLogger(logger)
{
	LoadShaderData(device);
}

D3D12::ShaderManager::~ShaderManager()
{
}

const void* D3D12::ShaderManager::GetGBufferVertexShaderData() const
{
	return mGBufferVertexShaderBlob->GetBufferPointer();
}

const void* D3D12::ShaderManager::GetGBufferPixelShaderData() const
{
	return mGBufferPixelShaderBlob->GetBufferPointer();
}

size_t D3D12::ShaderManager::GetGBufferVertexShaderSize() const
{
	return mGBufferVertexShaderBlob->GetBufferSize();
}

size_t D3D12::ShaderManager::GetGBufferPixelShaderSize() const
{
	return mGBufferPixelShaderBlob->GetBufferSize();
}

ID3D12RootSignature* D3D12::ShaderManager::GetGBufferRootSignature() const
{
	return mGBufferRootSignature.get();
}

void D3D12::ShaderManager::BuildGBufferRootSignature(ID3D12Device* device, ID3D12ShaderReflection* vsReflection, ID3D12ShaderReflection* psReflection)
{
	//Sorted from most frequent to last frequent
	std::array<std::string_view, 6> shaderInputs;
	shaderInputs[GBufferMaterialIndexBinding]   = "cbMaterialIndex";
	shaderInputs[GBufferObjectIndexBinding]     = "cbObjectIndex";
	shaderInputs[GBufferPerObjectBufferBinding] = "cbPerObject";
	shaderInputs[GBufferPerFrameBufferBinding]  = "cbPerFrame";
	shaderInputs[GBufferMaterialBinding]        = "cbMaterialData";
	shaderInputs[GBufferTextureBinding]         = "gObjectTexture";

	std::array<D3D12_ROOT_PARAMETER_TYPE, shaderInputs.size()> shaderInputTypes;
	shaderInputTypes[GBufferMaterialIndexBinding]   = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	shaderInputTypes[GBufferObjectIndexBinding]     = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	shaderInputTypes[GBufferPerObjectBufferBinding] = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	shaderInputTypes[GBufferPerFrameBufferBinding]  = D3D12_ROOT_PARAMETER_TYPE_CBV;
	shaderInputTypes[GBufferMaterialBinding]        = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	shaderInputTypes[GBufferTextureBinding]         = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	
	D3D12_ROOT_SIGNATURE_FLAGS rootSigFlags = CreateDefaultRootSignatureFlags();
	std::unordered_map<std::string, D3D12_SHADER_INPUT_BIND_DESC> bindingInfos;
	std::unordered_map<std::string, D3D12_SHADER_VISIBILITY>      visibilities;
	CollectBindingInfos(vsReflection, bindingInfos, visibilities, &rootSigFlags);
	CollectBindingInfos(psReflection, bindingInfos, visibilities, &rootSigFlags);

	CreateRootSignature(device, bindingInfos, visibilities, shaderInputs, "gSamplers", shaderInputTypes, rootSigFlags, mGBufferRootSignature.put());
}

void D3D12::ShaderManager::LoadShaderData(ID3D12Device* device)
{
	wil::com_ptr_nothrow<IDxcUtils> dxcUtils;
	THROW_IF_FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(dxcUtils.put())));


	mGBufferVertexShaderBlob.reset();
	mGBufferPixelShaderBlob.reset();
	const std::wstring gbufferShaderDir = Utils::GetMainDirectory() + L"Shaders/D3D12/GBuffer/";
	
	THROW_IF_FAILED(dxcUtils->LoadFile((gbufferShaderDir + L"GBufferDrawVS.cso").c_str(), nullptr, mGBufferVertexShaderBlob.put()));
	THROW_IF_FAILED(dxcUtils->LoadFile((gbufferShaderDir + L"GBufferDrawPS.cso").c_str(), nullptr, mGBufferPixelShaderBlob.put()));

	wil::com_ptr_nothrow<ID3D12ShaderReflection> gbufferVsReflection;
	CreateReflectionData(dxcUtils.get(), mGBufferVertexShaderBlob.get(), gbufferVsReflection.put());

	wil::com_ptr_nothrow<ID3D12ShaderReflection> gbufferPsReflection;
	CreateReflectionData(dxcUtils.get(), mGBufferPixelShaderBlob.get(), gbufferPsReflection.put());


	BuildGBufferRootSignature(device, gbufferVsReflection.get(), gbufferPsReflection.get());
}

void D3D12::ShaderManager::CreateReflectionData(IDxcUtils* dxcUtils, IDxcBlobEncoding* pBlob, ID3D12ShaderReflection** outShaderReflection) const
{
	BOOL   endodingKnown = FALSE;
	UINT32 encoding      = 0;
	THROW_IF_FAILED(pBlob->GetEncoding(&endodingKnown, &encoding));

	DxcBuffer dxcBuffer;
	dxcBuffer.Encoding = encoding;
	dxcBuffer.Ptr      = pBlob->GetBufferPointer();
	dxcBuffer.Size     = pBlob->GetBufferSize();

	THROW_IF_FAILED(dxcUtils->CreateReflection(&dxcBuffer, IID_PPV_ARGS(outShaderReflection)));
}

void D3D12::ShaderManager::CollectBindingInfos(ID3D12ShaderReflection* reflection, std::unordered_map<std::string, D3D12_SHADER_INPUT_BIND_DESC>& outBindingInfos, std::unordered_map<std::string, D3D12_SHADER_VISIBILITY>& outShaderVisibility, D3D12_ROOT_SIGNATURE_FLAGS* rootSignatureFlags) const
{
	assert(rootSignatureFlags != nullptr);

	D3D12_SHADER_DESC shaderDesc;
	THROW_IF_FAILED(reflection->GetDesc(&shaderDesc));

	D3D12_SHADER_VERSION_TYPE versionType = (D3D12_SHADER_VERSION_TYPE)((shaderDesc.Version & 0xFFFF0000) >> 16);
	D3D12_SHADER_VISIBILITY visibility    = D3D12_SHADER_VISIBILITY_ALL;
	switch(versionType)
	{
	case D3D12_SHVER_VERTEX_SHADER:
		visibility = D3D12_SHADER_VISIBILITY_VERTEX;

		*rootSignatureFlags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
		if(shaderDesc.InputParameters > 0)
		{
			*rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		}

		break;
	case D3D12_SHVER_PIXEL_SHADER:
		visibility = D3D12_SHADER_VISIBILITY_PIXEL;
		*rootSignatureFlags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
		break;
	case D3D12_SHVER_GEOMETRY_SHADER:
		visibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
		*rootSignatureFlags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
		break;
	case D3D12_SHVER_HULL_SHADER:
		visibility = D3D12_SHADER_VISIBILITY_HULL;
		*rootSignatureFlags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
		break;
	case D3D12_SHVER_DOMAIN_SHADER:
		visibility = D3D12_SHADER_VISIBILITY_DOMAIN;
		*rootSignatureFlags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
		break;
	case D3D12_SHVER_COMPUTE_SHADER:
		visibility = D3D12_SHADER_VISIBILITY_ALL;
		break;
	case D3D12_SHVER_MESH_SHADER:
		visibility = D3D12_SHADER_VISIBILITY_MESH;
		*rootSignatureFlags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS;
		break;
	case D3D12_SHVER_AMPLIFICATION_SHADER:
		visibility = D3D12_SHADER_VISIBILITY_AMPLIFICATION;
		*rootSignatureFlags &= ~D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS;
		break;
	default:
		visibility = D3D12_SHADER_VISIBILITY_ALL;
		break;
	}

	for(UINT bindIndex = 0; bindIndex < shaderDesc.BoundResources; bindIndex++)
	{
		D3D12_SHADER_INPUT_BIND_DESC bindingDesc;
		THROW_IF_FAILED(reflection->GetResourceBindingDesc(bindIndex, &bindingDesc));

		outBindingInfos[bindingDesc.Name] = bindingDesc;

		auto bindingVisibilityIt = outShaderVisibility.find(bindingDesc.Name);
		if(bindingVisibilityIt != outShaderVisibility.end())
		{
			bindingVisibilityIt->second = D3D12_SHADER_VISIBILITY_ALL;
		}
		else
		{
			outShaderVisibility[bindingDesc.Name] = visibility;
		}
	}
}

void D3D12::ShaderManager::CreateRootSignature(ID3D12Device* device, const std::unordered_map<std::string, D3D12_SHADER_INPUT_BIND_DESC>& bindingInfos, const std::unordered_map<std::string, D3D12_SHADER_VISIBILITY>& visibilities, std::span<std::string_view> shaderInputNames, std::string_view samplersInputName, std::span<D3D12_ROOT_PARAMETER_TYPE> shaderInputTypes, D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags, ID3D12RootSignature** outRootSignature) const
{
	assert(shaderInputTypes.size() == shaderInputNames.size());

	//Create root signature
	std::vector<D3D12_ROOT_PARAMETER1>   rootParameters;
	std::vector<D3D12_DESCRIPTOR_RANGE1> rootDescriptorRanges;
	for(size_t i = 0; i = shaderInputNames.size(); i++)
	{
		D3D12_ROOT_PARAMETER1 rootParameter;
		rootParameter.ParameterType    = shaderInputTypes[i];
		rootParameter.ShaderVisibility = visibilities.at(std::string(shaderInputNames[i]));

		const D3D12_SHADER_INPUT_BIND_DESC& bindDesc = bindingInfos.at(std::string(shaderInputNames[i]));
		switch(shaderInputTypes[i])
		{
		case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
			D3D12_DESCRIPTOR_RANGE1 descrptorRange = CreateRootDescriptorRange(bindDesc, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
			rootDescriptorRanges.push_back(descrptorRange);
			rootParameter.DescriptorTable.NumDescriptorRanges = 1;
			rootParameter.DescriptorTable.pDescriptorRanges   = rootDescriptorRanges.data() + (rootDescriptorRanges.size() - 1);
			break;

		case D3D12_ROOT_PARAMETER_TYPE_CBV:
		case D3D12_ROOT_PARAMETER_TYPE_SRV:
		case D3D12_ROOT_PARAMETER_TYPE_UAV:
			rootParameter.Descriptor = CreateRootDescriptor(bindDesc, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
			break;

		case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
			rootParameter.Constants = CreateRootConstants(bindDesc, 1);
			break;

		default:
			break;
		}
	}

	//Gather sampler inputs
	D3D12_SHADER_VISIBILITY      samplersVisibility   = visibilities.at(std::string(samplersInputName));
	D3D12_SHADER_INPUT_BIND_DESC samplersInputBinding = bindingInfos.at(std::string(samplersInputName));

	std::array<D3D12_FILTER, StaticSamplerCount> samplerFilters = {D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_FILTER_ANISOTROPIC};

	std::array<D3D12_STATIC_SAMPLER_DESC, StaticSamplerCount> staticSamplers;
	for(size_t i = 0; i < samplerFilters.size(); i++)
	{
		staticSamplers[i].Filter           = samplerFilters[i];
		staticSamplers[i].AddressU         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSamplers[i].AddressV         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSamplers[i].AddressW         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSamplers[i].MipLODBias       = 0.0f;
		staticSamplers[i].MaxAnisotropy    = 0;
		staticSamplers[i].ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
		staticSamplers[i].BorderColor      = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
		staticSamplers[i].MinLOD           = 0.0f;
		staticSamplers[i].MaxLOD           = D3D12_FLOAT32_MAX;
		staticSamplers[i].ShaderRegister   = samplersInputBinding.BindPoint + (UINT)i;
		staticSamplers[i].RegisterSpace    = samplersInputBinding.Space;
		staticSamplers[i].ShaderVisibility = samplersVisibility;
	}


	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedRootSignatureDesc;
	versionedRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	versionedRootSignatureDesc.Desc_1_1.NumParameters     = (UINT)rootParameters.size();
	versionedRootSignatureDesc.Desc_1_1.pParameters       = rootParameters.data();
	versionedRootSignatureDesc.Desc_1_1.NumStaticSamplers = (UINT)staticSamplers.size();
	versionedRootSignatureDesc.Desc_1_1.pStaticSamplers   = staticSamplers.data();
	versionedRootSignatureDesc.Desc_1_1.Flags             = rootSignatureFlags;

	wil::com_ptr_nothrow<ID3DBlob> rootSignatureBlob;
	wil::com_ptr_nothrow<ID3DBlob> rootSignatureErrorBlob;
	THROW_IF_FAILED(D3D12SerializeVersionedRootSignature(&versionedRootSignatureDesc, rootSignatureBlob.put(), rootSignatureErrorBlob.put()));

	//TODO: mGPU?
	THROW_IF_FAILED(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(outRootSignature)));
}

D3D12_ROOT_SIGNATURE_FLAGS D3D12::ShaderManager::CreateDefaultRootSignatureFlags() const
{
	return D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS
		 | D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS
		 | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
		 | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
		 | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
		 | D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS
		 | D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS;
}

D3D12_ROOT_CONSTANTS D3D12::ShaderManager::CreateRootConstants(const D3D12_SHADER_INPUT_BIND_DESC& inputBindDesc, UINT numConstants) const
{
	assert(inputBindDesc.Type == D3D_SIT_CBUFFER);

	return D3D12_ROOT_CONSTANTS
	{
		.ShaderRegister = inputBindDesc.BindPoint,
		.RegisterSpace  = inputBindDesc.Space,
		.Num32BitValues = numConstants
	};
}

D3D12_ROOT_DESCRIPTOR1 D3D12::ShaderManager::CreateRootDescriptor(const D3D12_SHADER_INPUT_BIND_DESC& inputBindDesc, D3D12_ROOT_DESCRIPTOR_FLAGS flags) const
{
	assert(inputBindDesc.Type != D3D_SIT_TEXTURE);
	assert(inputBindDesc.Type != D3D_SIT_SAMPLER);
	assert(inputBindDesc.Type != D3D_SIT_RTACCELERATIONSTRUCTURE);

	return D3D12_ROOT_DESCRIPTOR1
	{
		.ShaderRegister = inputBindDesc.BindPoint,
		.RegisterSpace  = inputBindDesc.Space,
		.Flags          = flags
	};
}

D3D12_DESCRIPTOR_RANGE1 D3D12::ShaderManager::CreateRootDescriptorRange(const D3D12_SHADER_INPUT_BIND_DESC& inputBindDesc, UINT offsetFromTableStart, D3D12_DESCRIPTOR_RANGE_FLAGS flags) const
{
	D3D12_DESCRIPTOR_RANGE_TYPE rangeType;
	switch(inputBindDesc.Type)
	{
	case D3D_SIT_CBUFFER:
		rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		break;

    case D3D_SIT_TBUFFER:
	case D3D_SIT_TEXTURE:
	case D3D_SIT_STRUCTURED:
	case D3D_SIT_BYTEADDRESS:
	case D3D_SIT_RTACCELERATIONSTRUCTURE:
		rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		break;

	case D3D_SIT_UAV_RWTYPED:
	case D3D_SIT_UAV_RWSTRUCTURED:
	case D3D_SIT_UAV_RWBYTEADDRESS:
	case D3D_SIT_UAV_APPEND_STRUCTURED:
	case D3D_SIT_UAV_CONSUME_STRUCTURED:
	case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
	case D3D_SIT_UAV_FEEDBACKTEXTURE:
		rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		break;

    case D3D_SIT_SAMPLER:
		rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
		break;

	default:
		rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		break;
	}

	return D3D12_DESCRIPTOR_RANGE1
	{
		.RangeType                         = rangeType,
		.NumDescriptors                    = inputBindDesc.BindCount,
		.BaseShaderRegister                = inputBindDesc.BindPoint,
		.RegisterSpace                     = inputBindDesc.Space,
		.Flags                             = flags,
		.OffsetInDescriptorsFromTableStart = offsetFromTableStart
	};
}