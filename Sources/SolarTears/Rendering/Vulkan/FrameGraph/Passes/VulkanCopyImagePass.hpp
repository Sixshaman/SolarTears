#pragma once

#include "../VulkanRenderPass.hpp"

class FrameGraphConfig;

namespace Vulkan
{
	class FrameGraphBuilder;

	class CopyImagePass: public RenderPass
	{
	public:
		static constexpr std::string_view SrcImageId = "CopyImagePass-SrcImage";
		static constexpr std::string_view DstImageId = "CopyImagePass-DstImage";

	public:
		static void Register(FrameGraphBuilder* frameGraphBuilder, const std::string& passName);
		~CopyImagePass();

		void RecordExecution(VkCommandBuffer commandBuffer, const RenderableScene* scene, const FrameGraphConfig& frameGraphConfig) const override;

	private:
		CopyImagePass(VkDevice device, const FrameGraphBuilder* frameGraphBuilder, const std::string& currentPassName, uint32_t frame);

	private:
		VkImage mSrcImageRef;
		VkImage mDstImageRef;

		VkImageAspectFlags mSrcImageAspectFlags;
		VkImageAspectFlags mDstImageAspectFlags;
	};
}