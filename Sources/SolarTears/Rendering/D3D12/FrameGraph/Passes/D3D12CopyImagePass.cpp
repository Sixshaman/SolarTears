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

void D3D12::CopyImagePass::RegisterResources(FrameGraphBuilder* frameGraphBuilder, const std::string& passName)
{
	CopyImagePassBase::RegisterResources(frameGraphBuilder, passName);

	frameGraphBuilder->SetPassSubresourceState(SrcImageId, D3D12_RESOURCE_STATE_COPY_SOURCE);
	frameGraphBuilder->SetPassSubresourceState(DstImageId, D3D12_RESOURCE_STATE_COPY_DEST);
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