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

void D3D12::ShaderManager::CreateReflectionData(IDxcUtils* dxcUtils, IDxcBlobEncoding* pBlob, ID3D12ShaderReflection** outShaderReflection)
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

void D3D12::ShaderManager::BuildGBufferRootSignature(ID3D12Device* device, ID3D12ShaderReflection* vsReflection, ID3D12ShaderReflection* psReflection)
{
	//Sorted from least frequent to most frequent
	std::array<std::string_view, 3> shaderInputs;
	shaderInputs[GBufferPerObjectBufferBinding] = "cbPerObject";
	shaderInputs[GBufferPerFrameBufferBinding]  = "cbPerFrame";
	shaderInputs[GBufferTextureBinding]         = "gObjectTexture";

	//Gather regular inputs
	std::vector<D3D12_SHADER_INPUT_BIND_DESC> shaderInputBindings;
	std::vector<D3D12_SHADER_VISIBILITY>      shaderVisibilities;
	for(size_t i = 0; i < shaderInputs.size(); i++)
	{
		int shaderVisibility = 0;

		D3D12_SHADER_INPUT_BIND_DESC inputBindDesc;

		if(SUCCEEDED(vsReflection->GetResourceBindingDescByName(shaderInputs[i].data(), &inputBindDesc)))
		{
			if(shaderVisibility == 0)
			{
				shaderInputBindings.push_back(inputBindDesc);
			}

			shaderVisibility |= D3D12_SHADER_VISIBILITY_VERTEX;
		}

		if(SUCCEEDED(psReflection->GetResourceBindingDescByName(shaderInputs[i].data(), &inputBindDesc)))
		{
			if(shaderVisibility == 0)
			{
				shaderInputBindings.push_back(inputBindDesc);
			}

			shaderVisibility |= D3D12_SHADER_VISIBILITY_PIXEL;
		}

		shaderVisibilities.push_back((D3D12_SHADER_VISIBILITY)shaderVisibility);
	}


	//Gather sampler inputs
	std::string_view             samplersInput = "gSamplers";
	D3D12_SHADER_VISIBILITY      samplersVisibility;
	D3D12_SHADER_INPUT_BIND_DESC samplersInputBinding = {};

	{
		int samplersShaderVisibility = 0;

		D3D12_SHADER_INPUT_BIND_DESC inputBindDesc;

		if(SUCCEEDED(psReflection->GetResourceBindingDescByName(samplersInput.data(), &inputBindDesc)))
		{
			if(samplersShaderVisibility == 0)
			{
				samplersInputBinding = inputBindDesc;
			}

			samplersShaderVisibility |= D3D12_SHADER_VISIBILITY_VERTEX;
		}

		if(SUCCEEDED(psReflection->GetResourceBindingDescByName(samplersInput.data(), &inputBindDesc)))
		{
			if(samplersShaderVisibility == 0)
			{
				samplersInputBinding = inputBindDesc;
			}

			samplersShaderVisibility |= D3D12_SHADER_VISIBILITY_PIXEL;
		}

		samplersVisibility = (D3D12_SHADER_VISIBILITY)samplersShaderVisibility;
	}

	//Create root signature
	std::vector<D3D12_ROOT_PARAMETER1>   rootParameters;
	std::vector<D3D12_DESCRIPTOR_RANGE1> rootDescriptorRanges;
	for(size_t i = 0; i < shaderInputs.size(); i++)
	{
		D3D12_ROOT_PARAMETER1 rootParameter;

		if(shaderInputBindings[i].Type == D3D_SIT_TEXTURE)
		{
			rootDescriptorRanges.emplace_back();
			rootDescriptorRanges.back().RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			rootDescriptorRanges.back().NumDescriptors                    = 1;
			rootDescriptorRanges.back().BaseShaderRegister                = shaderInputBindings[i].BindPoint;
			rootDescriptorRanges.back().RegisterSpace                     = shaderInputBindings[i].Space;
			rootDescriptorRanges.back().OffsetInDescriptorsFromTableStart = 0;


			rootParameter.ParameterType    = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameter.ShaderVisibility = shaderVisibilities[i];
			rootParameter.DescriptorTable.NumDescriptorRanges = 1;
			rootParameter.DescriptorTable.pDescriptorRanges   = rootDescriptorRanges.data() + (rootDescriptorRanges.size() - 1);
		}
		else if(shaderInputBindings[i].Type == D3D_SIT_CBUFFER)
		{
			rootParameter.ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
			rootParameter.ShaderVisibility          = shaderVisibilities[i];
			rootParameter.Descriptor.ShaderRegister = shaderInputBindings[i].BindPoint;
			rootParameter.Descriptor.RegisterSpace  = shaderInputBindings[i].Space;
			rootParameter.Descriptor.Flags          = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
		}

		rootParameters.push_back(rootParameter);
	}


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
	versionedRootSignatureDesc.Desc_1_1.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		                                                  | D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS 
		                                                  | D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS
		                                                  | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
		                                                  | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
		                                                  | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	wil::com_ptr_nothrow<ID3DBlob> rootSignatureBlob;
	wil::com_ptr_nothrow<ID3DBlob> rootSignatureErrorBlob;
	THROW_IF_FAILED(D3D12SerializeVersionedRootSignature(&versionedRootSignatureDesc, rootSignatureBlob.put(), rootSignatureErrorBlob.put()));

	//TODO: mGPU?
	THROW_IF_FAILED(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(mGBufferRootSignature.put())));
}