#pragma once

#include "../VulkanRenderPass.hpp"
#include "../../../Common/FrameGraph/Passes/CopyImagePass.hpp"
#include "../VulkanFrameGraphMisc.hpp"
#include <span>

class FrameGraphConfig;

namespace Vulkan
{
	class FrameGraphBuilder;
	class ShaderDatabase;

	class CopyImagePass: public RenderPass, public CopyImagePassBase
	{
	public:
		inline static void RegisterSubresources(std::span<SubresourceMetadataPayload> inoutMetadataPayloads);
		inline static void RegisterShaders(ShaderDatabase* shaderDatabase);

	public:
		CopyImagePass(const FrameGraphBuilder* frameGraphBuilder, const std::string& currentPassName, uint32_t frame);
		~CopyImagePass();

		void RecordExecution(VkCommandBuffer commandBuffer, const RenderableScene* scene, const FrameGraphConfig& frameGraphConfig) const override;

	private:
		VkImage mSrcImageRef;
		VkImage mDstImageRef;

		VkImageAspectFlags mSrcImageAspectFlags;
		VkImageAspectFlags mDstImageAspectFlags;
	};
}

#include "VulkanCopyImagePass.inl"