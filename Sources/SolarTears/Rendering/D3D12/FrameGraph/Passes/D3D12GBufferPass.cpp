#include "D3D12GBufferPass.hpp"
#include "../../D3D12DeviceFeatures.hpp"
#include "../../D3D12Shaders.hpp"
#include "../../Scene/D3D12SceneBuilder.hpp"
#include "../D3D12FrameGraph.hpp"
#include "../D3D12FrameGraphBuilder.hpp"
#include "../../../FrameGraphConfig.hpp"
#include <array>

D3D12::GBufferPass::GBufferPass(ID3D12Device8* device, const FrameGraphBuilder* frameGraphBuilder, const std::string& passName): mFrameGraphTextureIndex((uint32_t)(-1))
{
	//TODO: bundles
	CreatePipelineState(device, frameGraphBuilder->GetShaderManager(), frameGraphBuilder->GetConfig());
}

D3D12::GBufferPass::~GBufferPass()
{
}

void D3D12::GBufferPass::Register(FrameGraphBuilder* frameGraphBuilder, const std::string& passName)
{
	auto PassCreateFunc = [](ID3D12Device8* device, const FrameGraphBuilder* builder, const std::string& name) -> std::unique_ptr<RenderPass>
	{
		//Using new because make_unique can't access private constructor
		return std::unique_ptr<GBufferPass>(new GBufferPass(device, builder, name));
	};

	frameGraphBuilder->RegisterRenderPass(passName, PassCreateFunc, RenderPassType::GRAPHICS);

	frameGraphBuilder->RegisterWriteSubresource(passName, ColorBufferImageId);
	frameGraphBuilder->EnableSubresourceAutoBarrier(passName, ColorBufferImageId, false);
	frameGraphBuilder->SetPassSubresourceFormat(passName, ColorBufferImageId, ColorOutputFormat); //TODO: maybe passing the format????
	frameGraphBuilder->SetPassSubresourceState(passName, ColorBufferImageId, D3D12_RESOURCE_STATE_RENDER_TARGET);
}

void D3D12::GBufferPass::RecordExecution(ID3D12GraphicsCommandList6* commandList, const RenderableScene* scene, const ShaderManager* shaderManager, const FrameGraphConfig& frameGraphConfig) const
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

	commandList->SetGraphicsRootSignature(shaderManager->GetGBufferRootSignature());
	commandList->SetPipelineState(mPipelineState.get());

	scene->DrawObjectsOntoGBuffer(commandList, shaderManager);

	commandList->EndRenderPass();
}

ID3D12PipelineState* D3D12::GBufferPass::FirstPipeline() const
{
	return mPipelineState.get();
}

void D3D12::GBufferPass::CreatePipelineState(ID3D12Device8* device, const ShaderManager* shaderManager, const FrameGraphConfig* frameGraphConfig)
{
	D3D12Utils::StateSubobjectHelper stateSubobjectHelper;

	stateSubobjectHelper.AddSubobjectGeneric(shaderManager->GetGBufferRootSignature());

	stateSubobjectHelper.AddVertexShader(shaderManager->GetGBufferVertexShaderData(), shaderManager->GetGBufferVertexShaderSize());
	stateSubobjectHelper.AddPixelShader(shaderManager->GetGBufferPixelShaderData(), shaderManager->GetGBufferPixelShaderSize());

	stateSubobjectHelper.AddSampleMask(0xffffffff);

	std::array<D3D12_INPUT_ELEMENT_DESC, 3> vertexInputDescs;
	vertexInputDescs[0].SemanticName         = "POSITION";
	vertexInputDescs[0].SemanticIndex        = 0;
	vertexInputDescs[0].Format               = RenderableSceneBuilder::GetVertexPositionFormat();
	vertexInputDescs[0].InputSlot            = 0;
	vertexInputDescs[0].AlignedByteOffset    = RenderableSceneBuilder::GetVertexPositionOffset();
	vertexInputDescs[0].InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	vertexInputDescs[0].InstanceDataStepRate = 0;
	vertexInputDescs[1].SemanticName         = "NORMAL";
	vertexInputDescs[1].SemanticIndex        = 0;
	vertexInputDescs[1].Format               = RenderableSceneBuilder::GetVertexNormalFormat();
	vertexInputDescs[1].InputSlot            = 0;
	vertexInputDescs[1].AlignedByteOffset    = RenderableSceneBuilder::GetVertexNormalOffset();
	vertexInputDescs[1].InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	vertexInputDescs[1].InstanceDataStepRate = 0;
	vertexInputDescs[2].SemanticName         = "TEXCOORD";
	vertexInputDescs[2].SemanticIndex        = 0;
	vertexInputDescs[2].Format               = RenderableSceneBuilder::GetVertexTexcoordFormat();
	vertexInputDescs[2].InputSlot            = 0;
	vertexInputDescs[2].AlignedByteOffset    = RenderableSceneBuilder::GetVertexTexcoordOffset();
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
	renderTargetFormats.RTFormats[0]     = ColorOutputFormat;
	renderTargetFormats.RTFormats[1]     = DXGI_FORMAT_UNKNOWN;
	renderTargetFormats.RTFormats[2]     = DXGI_FORMAT_UNKNOWN;
	renderTargetFormats.RTFormats[3]     = DXGI_FORMAT_UNKNOWN;
	renderTargetFormats.RTFormats[4]     = DXGI_FORMAT_UNKNOWN;
	renderTargetFormats.RTFormats[5]     = DXGI_FORMAT_UNKNOWN;
	renderTargetFormats.RTFormats[6]     = DXGI_FORMAT_UNKNOWN;
	renderTargetFormats.RTFormats[7]     = DXGI_FORMAT_UNKNOWN;

	stateSubobjectHelper.AddSubobjectGeneric(renderTargetFormats);
	
	//TODO: mGPU?
	stateSubobjectHelper.AddNodeMask(0);

	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc;
	pipelineStateStreamDesc.pPipelineStateSubobjectStream = stateSubobjectHelper.GetStreamPointer();
	pipelineStateStreamDesc.SizeInBytes                   = stateSubobjectHelper.GetStreamSize();

	THROW_IF_FAILED(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(mPipelineState.put())));
}