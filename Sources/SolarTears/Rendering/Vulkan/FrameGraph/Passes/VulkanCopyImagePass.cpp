#include "VulkanCopyImagePass.hpp"
#include "../../VulkanFunctions.hpp"
#include "../../../Common/FrameGraph/FrameGraphConfig.hpp"
#include "../VulkanFrameGraphBuilder.hpp"
#include <array>

Vulkan::CopyImagePass::CopyImagePass(const FrameGraphBuilder* frameGraphBuilder, uint32_t frameGraphPassId, uint32_t frame): RenderPass(frameGraphBuilder->GetDevice())
{
	mSrcImageRef = frameGraphBuilder->GetRegisteredResource(frameGraphPassId, (uint_fast16_t)PassSubresourceId::SrcImage, frame);
	mDstImageRef = frameGraphBuilder->GetRegisteredResource(frameGraphPassId, (uint_fast16_t)PassSubresourceId::DstImage, frame);

	mSrcImageAspectFlags = frameGraphBuilder->GetRegisteredSubresourceAspectFlags(frameGraphPassId, (uint_fast16_t)PassSubresourceId::SrcImage);
	mDstImageAspectFlags = frameGraphBuilder->GetRegisteredSubresourceAspectFlags(frameGraphPassId, (uint_fast16_t)PassSubresourceId::DstImage);
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

void Vulkan::CopyImagePass::ValidateDescriptorSets([[maybe_unused]] const ShaderDatabase* shaderDatabase, [[maybe_unused]] DescriptorDatabase* descriptorDatabase)
{
	//No descriptors are handled in this pass
}
