#include "D3D12CopyImagePass.hpp"
#include "../../../Common/FrameGraph/FrameGraphConfig.hpp"
#include "../D3D12FrameGraphBuilder.hpp"
#include <array>

D3D12::CopyImagePass::CopyImagePass(const FrameGraphBuilder* frameGraphBuilder, uint32_t passIndex)
{
	mSrcImageRef = frameGraphBuilder->GetRegisteredResource(passIndex, (uint_fast16_t)PassSubresourceId::SrcImage);
	mDstImageRef = frameGraphBuilder->GetRegisteredResource(passIndex, (uint_fast16_t)PassSubresourceId::DstImage);
}

D3D12::CopyImagePass::~CopyImagePass()
{
}

void D3D12::CopyImagePass::RecordExecution(ID3D12GraphicsCommandList6* commandList, [[maybe_unused]] const RenderableScene* scene, const FrameGraphConfig& frameGraphConfig, [[maybe_unused]] uint32_t frameResourceIndex) const
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

UINT D3D12::CopyImagePass::GetPassDescriptorCountNeeded()
{
	//No specific pass descriptors for this pass
	return 0;
}

void D3D12::CopyImagePass::ValidatePassDescriptors([[maybe_unused]] D3D12_GPU_DESCRIPTOR_HANDLE prevHeapStart, [[maybe_unused]] D3D12_GPU_DESCRIPTOR_HANDLE newHeapStart)
{
	//No specific pass descriptors for this pass
}

void D3D12::CopyImagePass::RequestSceneDescriptors([[maybe_unused]] DescriptorCreator* sceneDescriptorCreator)
{
	//No specific scene descriptors for this pass
}

void D3D12::CopyImagePass::ValidateSceneDescriptors([[maybe_unused]] const DescriptorCreator* sceneDescriptorCreator)
{
	//No specific scene descriptors for this pass
}

ID3D12PipelineState* D3D12::CopyImagePass::FirstPipeline() const
{
	return nullptr; //No pipeline is used by that pass
}