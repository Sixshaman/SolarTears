#include "D3D12GBufferPass.hpp"
#include "../../D3D12DeviceFeatures.hpp"
#include "../../D3D12Shaders.hpp"
#include "../../Scene/D3D12SceneBuilder.hpp"
#include "../D3D12FrameGraph.hpp"
#include "../D3D12FrameGraphBuilder.hpp"
#include "../../../Common/FrameGraph/FrameGraphConfig.hpp"
#include "../../../../Core/Util.hpp"
#include <array>

D3D12::GBufferPass::GBufferPass(ID3D12Device8* device, const FrameGraphBuilder* frameGraphBuilder, const std::string& passName, uint32_t frame)
{
	wil::com_ptr_t<IDxcBlobEncoding> staticVertexShaderBlob;
	wil::com_ptr_t<IDxcBlobEncoding> rigidVertexShaderBlob;
	wil::com_ptr_t<IDxcBlobEncoding> pixelShaderBlob;
	LoadShaders(frameGraphBuilder->GetShaderManager(), staticVertexShaderBlob.put(), rigidVertexShaderBlob.put(), pixelShaderBlob.put());

	CreateRootSignature(frameGraphBuilder->GetShaderManager(), device, rigidVertexShaderBlob.get(), pixelShaderBlob.get());

	//TODO: bundles
	CreateGBufferPipelineState(device, staticVertexShaderBlob.get(), pixelShaderBlob.get(), mStaticPipelineState.put());
	CreateGBufferPipelineState(device, rigidVertexShaderBlob.get(),  pixelShaderBlob.get(), mRigidPipelineState.put());

	mColorsRenderTarget = frameGraphBuilder->GetRegisteredSubresourceRtv(passName, ColorBufferImageId, frame);

	const FrameGraphConfig* frameGraphConfig = frameGraphBuilder->GetConfig();

	mViewport.TopLeftX = 0.0f;
	mViewport.TopLeftY = 0.0f;
	mViewport.Width    = (FLOAT)frameGraphConfig->GetViewportWidth();
	mViewport.Height   = (FLOAT)frameGraphConfig->GetViewportHeight();
	mViewport.MinDepth = 0.0f;
	mViewport.MaxDepth = 1.0f;

	mScissorRect.left   = 0;
	mScissorRect.top    = 0;
	mScissorRect.right  = frameGraphConfig->GetViewportWidth();
	mScissorRect.bottom = frameGraphConfig->GetViewportHeight();
}

D3D12::GBufferPass::~GBufferPass()
{
}

void D3D12::GBufferPass::OnAdd(FrameGraphBuilder* frameGraphBuilder, const std::string& passName)
{
	GBufferPassBase::OnAdd(frameGraphBuilder, passName);

	frameGraphBuilder->SetPassSubresourceFormat(passName, ColorBufferImageId, ColorOutputFormat); //TODO: maybe passing the format????
	frameGraphBuilder->SetPassSubresourceState(passName, ColorBufferImageId, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void D3D12::GBufferPass::RecordExecution(ID3D12GraphicsCommandList6* commandList, const RenderableScene* scene, const ShaderManager* shaderManager, [[maybe_unused]] const FrameGraphConfig& frameGraphConfig) const
{
	D3D12_RENDER_PASS_RENDER_TARGET_DESC colorsRtvDesc;
	colorsRtvDesc.cpuDescriptor                             = mColorsRenderTarget;
	colorsRtvDesc.BeginningAccess.Type                      = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
	colorsRtvDesc.BeginningAccess.Clear.ClearValue.Format   = ColorOutputFormat;
	colorsRtvDesc.BeginningAccess.Clear.ClearValue.Color[0] = 0.0f;
	colorsRtvDesc.BeginningAccess.Clear.ClearValue.Color[1] = 0.0f;
	colorsRtvDesc.BeginningAccess.Clear.ClearValue.Color[2] = 0.0f;
	colorsRtvDesc.BeginningAccess.Clear.ClearValue.Color[3] = 0.0f;
	colorsRtvDesc.EndingAccess.Type                         = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

	std::array renderPassRenderTargets = {colorsRtvDesc};

	//TODO: Bundles!
	//TODO: Suspending/resuming passes (in case of rendering parts of the scene with the same pass)
	commandList->BeginRenderPass((UINT)renderPassRenderTargets.size(), renderPassRenderTargets.data(), nullptr, D3D12_RENDER_PASS_FLAG_NONE);

	std::array viewports = {mViewport};
	commandList->RSSetViewports((UINT)viewports.size(), viewports.data());

	std::array scissorRects = {mScissorRect};
	commandList->RSSetScissorRects((UINT)scissorRects.size(), scissorRects.data());

	scene->PrepareDrawBuffers(commandList);
	commandList->SetGraphicsRootSignature(mRootSignature.get());

	constexpr auto perSubmeshFunction = [](ID3D12GraphicsCommandList* commandList, uint32_t materialIndex)
	{
		commandList->SetGraphicsRoot32BitConstant((UINT)GBufferRootBindings::MaterialIndex, materialIndex, 0);
	};

	constexpr auto perMeshFunction = [](ID3D12GraphicsCommandList* commandList, uint32_t objectDataIndex)
	{
		commandList->SetGraphicsRoot32BitConstant((UINT)GBufferRootBindings::ObjectIndex, objectDataIndex, 0);
	};

	commandList->SetGraphicsRootConstantBufferView((UINT)GBufferRootBindings::PerFrameBuffer, scene->GetCurrentFrameFrameData());

	commandList->SetGraphicsRootDescriptorTable((UINT)GBufferRootBindings::Materials, mSceneMaterialsTable);
	commandList->SetGraphicsRootDescriptorTable((UINT)GBufferRootBindings::Textures,  mSceneTexturesTable);

	commandList->SetPipelineState(mStaticPipelineState.get());
	scene->DrawStaticObjects(commandList, perSubmeshFunction);

	commandList->SetPipelineState(mRigidPipelineState.get());

	commandList->SetGraphicsRootDescriptorTable((UINT)GBufferRootBindings::PerObjectBuffers, mSceneStaticObjectsTable);
	scene->DrawStaticInstancedObjects(commandList, perMeshFunction, perSubmeshFunction);

	commandList->SetGraphicsRootDescriptorTable((UINT)GBufferRootBindings::PerObjectBuffers, mSceneRigidObjectsTable);
	scene->DrawRigidObjects(commandList, perMeshFunction, perSubmeshFunction);

	commandList->EndRenderPass();
}

ID3D12PipelineState* D3D12::GBufferPass::FirstPipeline() const
{
	return mStaticPipelineState.get();
}

consteval UINT D3D12::GBufferPass::GetPassDescriptorCountNeeded()
{
	return 0;
}

consteval UINT D3D12::GBufferPass::GetSceneDescriptorTypesNeeded()
{
	//Need descriptor tables for scene materials, textures and objects
	return (0x01u << (UINT)SceneDataType::ObjectData) | (0x01u << (UINT)SceneDataType::MaterialData) | (0x01u << (UINT)SceneDataType::TextureData);
}

void D3D12::GBufferPass::ValidatePassDescriptors([[maybe_unused]] D3D12_GPU_DESCRIPTOR_HANDLE prevHeapStart, [[maybe_unused]] D3D12_GPU_DESCRIPTOR_HANDLE newHeapStart)
{
}

void D3D12::GBufferPass::ValidateSceneDescriptors(const std::span<D3D12_GPU_DESCRIPTOR_HANDLE> newSceneDescriptorTables, const std::span<D3D12_GPU_VIRTUAL_ADDRESS> newSceneInlineDescriptors)
{
	assert(newSceneDescriptorTables.size()  >= (UINT)SceneDataType::Count);
	assert(newSceneInlineDescriptors.size() >= (UINT)SceneDataType::Count);

	mSceneObjectsTable   = newSceneDescriptorTables[(UINT)SceneDataType::ObjectData];
	mSceneMaterialsTable = newSceneDescriptorTables[(UINT)SceneDataType::MaterialData];
	mSceneTexturesTable  = newSceneDescriptorTables[(UINT)SceneDataType::TextureData];

	mSceneFrameDataBuffer = newSceneInlineDescriptors[(UINT)SceneDataType::FrameData];
}

void D3D12::GBufferPass::LoadShaders(const ShaderManager* shaderManager, IDxcBlobEncoding** outStaticVertexShader, IDxcBlobEncoding** outRigidVertexShader, IDxcBlobEncoding** outPixelShader)
{
	const std::wstring shaderFolder = Utils::GetMainDirectory() + L"Shaders/D3D12/GBuffer/";

	shaderManager->LoadShaderBlob(shaderFolder + L"GBufferDrawStaticVS.cso", outStaticVertexShader);
	shaderManager->LoadShaderBlob(shaderFolder + L"GBufferDrawRigidVS.cso",  outRigidVertexShader);
	shaderManager->LoadShaderBlob(shaderFolder + L"GBufferDrawPS.cso",       outPixelShader);
}

void D3D12::GBufferPass::CreateRootSignature(const ShaderManager* shaderManager, ID3D12Device8* device, IDxcBlobEncoding* vertexShader, IDxcBlobEncoding* pixelShader)
{
	//Sorted from most frequent to last frequent
	std::array<std::string_view, (UINT)GBufferRootBindings::Count> shaderInputs;
	shaderInputs[(UINT)GBufferRootBindings::MaterialIndex]    = "cbMaterialIndex";
	shaderInputs[(UINT)GBufferRootBindings::ObjectIndex]      = "cbObjectIndex";
	shaderInputs[(UINT)GBufferRootBindings::PerObjectBuffers] = "cbPerObject";
	shaderInputs[(UINT)GBufferRootBindings::PerFrameBuffer]   = "cbPerFrame";
	shaderInputs[(UINT)GBufferRootBindings::Materials]        = "cbMaterialData";
	shaderInputs[(UINT)GBufferRootBindings::Textures]         = "gObjectTexture";

	std::array<D3D12_ROOT_PARAMETER_TYPE, shaderInputs.size()> shaderInputTypes;
	shaderInputTypes[(UINT)GBufferRootBindings::MaterialIndex]    = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	shaderInputTypes[(UINT)GBufferRootBindings::ObjectIndex]      = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	shaderInputTypes[(UINT)GBufferRootBindings::PerObjectBuffers] = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	shaderInputTypes[(UINT)GBufferRootBindings::PerFrameBuffer]   = D3D12_ROOT_PARAMETER_TYPE_CBV;
	shaderInputTypes[(UINT)GBufferRootBindings::Materials]        = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	shaderInputTypes[(UINT)GBufferRootBindings::Textures]         = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	
	std::array shaderBlobs = {vertexShader, pixelShader};
	shaderManager->CreateRootSignature(device, shaderBlobs, shaderInputs, shaderInputTypes, mRootSignature.put());
}

void D3D12::GBufferPass::CreateGBufferPipelineState(ID3D12Device8* device, IDxcBlobEncoding* vertexShader, IDxcBlobEncoding* pixelShader, ID3D12PipelineState** outPipelineState)
{
	D3D12Utils::StateSubobjectHelper stateSubobjectHelper;

	stateSubobjectHelper.AddSubobjectGeneric(mRootSignature.get());

	stateSubobjectHelper.AddVertexShader(vertexShader->GetBufferPointer(), vertexShader->GetBufferSize());
	stateSubobjectHelper.AddPixelShader(pixelShader->GetBufferPointer(), pixelShader->GetBufferSize());

	stateSubobjectHelper.AddSampleMask(0xffffffff);

	std::array<D3D12_INPUT_ELEMENT_DESC, 3> vertexInputDescs;
	vertexInputDescs[0].SemanticName         = "POSITION";
	vertexInputDescs[0].SemanticIndex        = 0;
	vertexInputDescs[0].Format               = D3D12Utils::FormatForVectorType<decltype(RenderableSceneVertex::Position)>;
	vertexInputDescs[0].InputSlot            = 0;
	vertexInputDescs[0].AlignedByteOffset    = offsetof(RenderableSceneVertex, Position);
	vertexInputDescs[0].InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	vertexInputDescs[0].InstanceDataStepRate = 0;
	vertexInputDescs[1].SemanticName         = "NORMAL";
	vertexInputDescs[1].SemanticIndex        = 0;
	vertexInputDescs[1].Format               = D3D12Utils::FormatForVectorType<decltype(RenderableSceneVertex::Normal)>;
	vertexInputDescs[1].InputSlot            = 0;
	vertexInputDescs[1].AlignedByteOffset    = offsetof(RenderableSceneVertex, Normal);
	vertexInputDescs[1].InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	vertexInputDescs[1].InstanceDataStepRate = 0;
	vertexInputDescs[2].SemanticName         = "TEXCOORD";
	vertexInputDescs[2].SemanticIndex        = 0;
	vertexInputDescs[2].Format               = D3D12Utils::FormatForVectorType<decltype(RenderableSceneVertex::Texcoord)>;
	vertexInputDescs[2].InputSlot            = 0;
	vertexInputDescs[2].AlignedByteOffset    = offsetof(RenderableSceneVertex, Texcoord);
	vertexInputDescs[2].InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	vertexInputDescs[2].InstanceDataStepRate = 0;

	stateSubobjectHelper.AddSubobjectGeneric(D3D12_INPUT_LAYOUT_DESC
	{
		.pInputElementDescs = vertexInputDescs.data(),
		.NumElements        = (UINT)vertexInputDescs.size()
	});

	stateSubobjectHelper.AddSubobjectGeneric(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

	D3D12_RT_FORMAT_ARRAY renderTargetFormats;
	renderTargetFormats.NumRenderTargets = 1;
	renderTargetFormats.RTFormats[0] = ColorOutputFormat;
	renderTargetFormats.RTFormats[1] = DXGI_FORMAT_UNKNOWN;
	renderTargetFormats.RTFormats[2] = DXGI_FORMAT_UNKNOWN;
	renderTargetFormats.RTFormats[3] = DXGI_FORMAT_UNKNOWN;
	renderTargetFormats.RTFormats[4] = DXGI_FORMAT_UNKNOWN;
	renderTargetFormats.RTFormats[5] = DXGI_FORMAT_UNKNOWN;
	renderTargetFormats.RTFormats[6] = DXGI_FORMAT_UNKNOWN;
	renderTargetFormats.RTFormats[7] = DXGI_FORMAT_UNKNOWN;

	stateSubobjectHelper.AddSubobjectGeneric(renderTargetFormats);
	
	//TODO: mGPU?
	stateSubobjectHelper.AddNodeMask(0);

	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc;
	pipelineStateStreamDesc.pPipelineStateSubobjectStream = stateSubobjectHelper.GetStreamPointer();
	pipelineStateStreamDesc.SizeInBytes                   = stateSubobjectHelper.GetStreamSize();

	THROW_IF_FAILED(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(outPipelineState)));
}
