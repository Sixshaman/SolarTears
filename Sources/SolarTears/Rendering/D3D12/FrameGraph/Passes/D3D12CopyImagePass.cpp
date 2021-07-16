#include "D3D12CopyImagePass.hpp"
#include "../../../Common/FrameGraph/FrameGraphConfig.hpp"
#include "../D3D12FrameGraphBuilder.hpp"
#include <array>

D3D12::CopyImagePass::CopyImagePass([[maybe_unused]] ID3D12Device8* device, const FrameGraphBuilder* frameGraphBuilder, const std::string& currentPassName, uint32_t frame)
{
	mSrcImageRef = frameGraphBuilder->GetRegisteredResource(currentPassName, SrcImageId, frame);
	mDstImageRef = frameGraphBuilder->GetRegisteredResource(currentPassName, DstImageId, frame);
}

D3D12::CopyImagePass::~CopyImagePass()
{
}

void D3D12::CopyImagePass::OnAdd(FrameGraphBuilder* frameGraphBuilder, const std::string& passName)
{
	CopyImagePassBase::OnAdd(frameGraphBuilder, passName);

	frameGraphBuilder->SetPassSubresourceState(passName, SrcImageId, D3D12_RESOURCE_STATE_COPY_SOURCE);
	frameGraphBuilder->SetPassSubresourceState(passName, DstImageId, D3D12_RESOURCE_STATE_COPY_DEST);
}

void D3D12::CopyImagePass::RecordExecution(ID3D12GraphicsCommandList6* commandList, [[maybe_unused]] const RenderableScene* scene, [[maybe_unused]] const ShaderManager* shaderManager, const FrameGraphConfig& frameGraphConfig) const
{
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
	srcBox.top    = 0;
	srcBox.front  = 0;
	srcBox.right  = frameGraphConfig.GetViewportWidth();
	srcBox.bottom = frameGraphConfig.GetViewportHeight();
	srcBox.back   = 1;

	commandList->CopyTextureRegion(&dstRegion, 0, 0, 0, &srcRegion, &srcBox);
}

void D3D12::CopyImagePass::RevalidateSrvUavDescriptors([[maybe_unused]] D3D12_GPU_DESCRIPTOR_HANDLE prevHeapStart, [[maybe_unused]] D3D12_GPU_DESCRIPTOR_HANDLE newHeapStart)
{
}

ID3D12PipelineState* D3D12::CopyImagePass::FirstPipeline() const
{
	return nullptr; //No pipeline is used by that pass
}