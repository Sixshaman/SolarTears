#pragma once

#include "../VulkanRenderPass.hpp"
#include "../../../Common/FrameGraph/Passes/CopyImagePass.hpp"

class FrameGraphConfig;

namespace Vulkan
{
	class FrameGraphBuilder;

	class CopyImagePass: public RenderPass, public CopyImagePassBase
	{
	public:
		CopyImagePass(VkDevice device, const FrameGraphBuilder* frameGraphBuilder, const std::string& currentPassName, uint32_t frame);
		~CopyImagePass();

		void RecordExecution(VkCommandBuffer commandBuffer, const RenderableScene* scene, const FrameGraphConfig& frameGraphConfig) const override;

	public:
		static void OnAdd(FrameGraphBuilder* frameGraphBuilder, const std::string& passName);

	private:
		VkImage mSrcImageRef;
		VkImage mDstImageRef;

		VkImageAspectFlags mSrcImageAspectFlags;
		VkImageAspectFlags mDstImageAspectFlags;
	};
}