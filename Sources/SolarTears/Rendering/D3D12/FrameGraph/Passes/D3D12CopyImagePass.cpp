#include "D3D12CopyImagePass.hpp"
#include "../../../FrameGraphConfig.hpp"
#include "../D3D12FrameGraphBuilder.hpp"
#include <array>

D3D12::CopyImagePass::CopyImagePass(ID3D12Device8* device, const FrameGraphBuilder* frameGraphBuilder, const std::string& currentPassName)
{
	UNREFERENCED_PARAMETER(device);

	mSrcImageRef = frameGraphBuilder->GetRegisteredResource(currentPassName, SrcImageId);
	mDstImageRef = frameGraphBuilder->GetRegisteredResource(currentPassName, DstImageId);
}

void D3D12::CopyImagePass::Register(FrameGraphBuilder* frameGraphBuilder, const std::string& passName)
{
	auto PassCreateFunc = [](ID3D12Device8* device, const FrameGraphBuilder* builder, const std::string& name) -> std::unique_ptr<RenderPass>
	{
		//Using new because make_unique can't access private constructor
		return std::unique_ptr<CopyImagePass>(new CopyImagePass(device, builder, name));
	};

	frameGraphBuilder->RegisterRenderPass(passName, PassCreateFunc, RenderPassType::COPY);

	frameGraphBuilder->RegisterReadSubresource(passName,  SrcImageId);
	frameGraphBuilder->RegisterWriteSubresource(passName, DstImageId);
}

D3D12::CopyImagePass::~CopyImagePass()
{
}

void D3D12::CopyImagePass::RecordExecution(ID3D12GraphicsCommandList6* commandList, const RenderableScene* scene, const ShaderManager* shaderManager, const FrameGraphConfig& frameGraphConfig) const
{
	UNREFERENCED_PARAMETER(scene);
	UNREFERENCED_PARAMETER(shaderManager);

	D3D12_TEXTURE_COPY_LOCATION dstRegion;
	dstRegion.pResource        = mDstImageRef;
	dstRegion.SubresourceIndex = 0;
	dstRegion.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

	D3D12_TEXTURE_COPY_LOCATION srcRegion;
	srcRegion.pResource        = mSrcImageRef;
	srcRegion.SubresourceIndex = 0;
	srcRegion.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

	D3D12_BOX srcBox;
	srcBox.left   = 0;
	srcBox.bottom = 0;
	srcBox.back   = 0;
	srcBox.right  = frameGraphConfig.GetViewportWidth();
	srcBox.top    = frameGraphConfig.GetViewportHeight();
	srcBox.front  = 1;

	commandList->CopyTextureRegion(&dstRegion, 0, 0, 0, &srcRegion, &srcBox);
}

ID3D12PipelineState* D3D12::CopyImagePass::FirstPipeline() const
{
	return nullptr; //No pipeline is used by that pass
}