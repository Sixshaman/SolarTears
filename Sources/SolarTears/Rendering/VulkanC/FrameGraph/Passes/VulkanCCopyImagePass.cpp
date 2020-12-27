#include "VulkanCCopyImagePass.hpp"
#include "../../VulkanCFunctions.hpp"
#include "../VulkanCFrameGraphConfig.hpp"
#include "../VulkanCFrameGraphBuilder.hpp"
#include <array>

VulkanCBindings::CopyImagePass::CopyImagePass(VkDevice device, const FrameGraphBuilder* frameGraphBuilder, const std::string& currentPassName): RenderPass(device, RenderPassType::TRANSFER)
{
	mSrcImageRef = frameGraphBuilder->GetRegisteredResource(currentPassName, SrcImageId);
	mDstImageRef = frameGraphBuilder->GetRegisteredResource(currentPassName, DstImageId);

	mSrcImageAspectFlags = frameGraphBuilder->GetRegisteredSubresourceAspectFlags(currentPassName, SrcImageId);
	mDstImageAspectFlags = frameGraphBuilder->GetRegisteredSubresourceAspectFlags(currentPassName, DstImageId);
}

void VulkanCBindings::CopyImagePass::Register(FrameGraphBuilder* frameGraphBuilder, const std::string& passName)
{
	auto PassCreateFunc = [](VkDevice device, const FrameGraphBuilder* builder, const std::string& name) -> std::unique_ptr<RenderPass>
	{
		//Using new because make_unique can't access private constructor
		return std::unique_ptr<CopyImagePass>(new CopyImagePass(device, builder, name));
	};

	frameGraphBuilder->RegisterRenderPass(passName, PassCreateFunc);

	frameGraphBuilder->RegisterReadSubresource(passName, SrcImageId);
	frameGraphBuilder->SetPassSubresourceAspectFlags(passName, SrcImageId, VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
	frameGraphBuilder->SetPassSubresourceStageFlags(passName, SrcImageId, VK_PIPELINE_STAGE_TRANSFER_BIT);
	frameGraphBuilder->SetPassSubresourceLayout(passName, SrcImageId, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	frameGraphBuilder->SetPassSubresourceUsage(passName, SrcImageId, VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

	frameGraphBuilder->RegisterWriteSubresource(passName, DstImageId);
	frameGraphBuilder->SetPassSubresourceAspectFlags(passName, DstImageId, VK_IMAGE_ASPECT_COLOR_BIT | VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
	frameGraphBuilder->SetPassSubresourceStageFlags(passName, DstImageId, VK_PIPELINE_STAGE_TRANSFER_BIT);
	frameGraphBuilder->SetPassSubresourceLayout(passName, DstImageId, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	frameGraphBuilder->SetPassSubresourceUsage(passName, DstImageId, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
}

VulkanCBindings::CopyImagePass::~CopyImagePass()
{
}

void VulkanCBindings::CopyImagePass::RecordExecution(VkCommandBuffer commandBuffer, const RenderableScene* scene, const FrameGraphConfig& frameGraphConfig, uint32_t currentFrameResourceIndex)
{
	UNREFERENCED_PARAMETER(scene);
	UNREFERENCED_PARAMETER(currentFrameResourceIndex);

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