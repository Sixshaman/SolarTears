#include "VulkanCopyImagePass.hpp"
#include "../../VulkanFunctions.hpp"
#include "../../../Common/FrameGraph/FrameGraphConfig.hpp"
#include "../VulkanFrameGraphBuilder.hpp"
#include <array>

Vulkan::CopyImagePass::CopyImagePass(const FrameGraphBuilder* frameGraphBuilder, const std::string& currentPassName, uint32_t frame): RenderPass(frameGraphBuilder->GetDevice())
{
	mSrcImageRef = frameGraphBuilder->GetRegisteredResource(currentPassName, SrcImageId, frame);
	mDstImageRef = frameGraphBuilder->GetRegisteredResource(currentPassName, DstImageId, frame);

	mSrcImageAspectFlags = frameGraphBuilder->GetRegisteredSubresourceAspectFlags(SrcImageId);
	mDstImageAspectFlags = frameGraphBuilder->GetRegisteredSubresourceAspectFlags(DstImageId);
}

Vulkan::CopyImagePass::~CopyImagePass()
{
}

void Vulkan::CopyImagePass::RecordExecution(VkCommandBuffer commandBuffer, [[maybe_unused]] const RenderableScene* scene, const FrameGraphConfig& frameGraphConfig) const
{
	VkImageCopy imageCopyInfo;
	imageCopyInfo.srcOffset.x                   = 0;
	imageCopyInfo.srcOffset.y                   = 0;
	imageCopyInfo.srcOffset.z                   = 0;
	imageCopyInfo.dstOffset.x                   = 0;
	imageCopyInfo.dstOffset.y                   = 0;
	imageCopyInfo.dstOffset.z                   = 0;
	imageCopyInfo.extent.width                  = frameGraphConfig.GetViewportWidth();
	imageCopyInfo.extent.height                 = frameGraphConfig.GetViewportHeight();
	imageCopyInfo.extent.depth                  = 1;
	imageCopyInfo.srcSubresource.aspectMask     = mSrcImageAspectFlags;
	imageCopyInfo.srcSubresource.mipLevel       = 0;
	imageCopyInfo.srcSubresource.baseArrayLayer = 0;
	imageCopyInfo.srcSubresource.layerCount     = 1;
	imageCopyInfo.dstSubresource.aspectMask     = mDstImageAspectFlags;
	imageCopyInfo.dstSubresource.mipLevel       = 0;
	imageCopyInfo.dstSubresource.baseArrayLayer = 0;
	imageCopyInfo.dstSubresource.layerCount     = 1;

	std::array copyRegions = {imageCopyInfo};
	vkCmdCopyImage(commandBuffer, mSrcImageRef, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mDstImageRef, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)(copyRegions.size()), copyRegions.data());
}