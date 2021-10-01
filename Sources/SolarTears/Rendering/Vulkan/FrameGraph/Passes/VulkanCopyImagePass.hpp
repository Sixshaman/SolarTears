#pragma once

#include "../VulkanRenderPass.hpp"
#include "../../../Common/FrameGraph/Passes/CopyImagePass.hpp"
#include "../VulkanFrameGraphMisc.hpp"
#include <span>

class FrameGraphConfig;

namespace Vulkan
{
	class FrameGraphBuilder;

	class CopyImagePass: public RenderPass, public CopyImagePassBase
	{
	public:
		inline static void             RegisterSubresources(std::span<SubresourceMetadataPayload> inoutMetadataPayloads);
		inline static void             RegisterShaders(ShaderDatabase* shaderDatabase);
		inline static bool             PropagateSubresourceInfos(std::span<SubresourceMetadataPayload> inoutMetadataPayloads);
		inline static VkDescriptorType GetSubresourceDescriptorType(uint_fast16_t subresourceId);

	public:
		CopyImagePass(const FrameGraphBuilder* frameGraphBuilder, uint32_t frameGraphPassIndex, uint32_t frame);
		~CopyImagePass();

		void RecordExecution(VkCommandBuffer commandBuffer, const RenderableScene* scene, const FrameGraphConfig& frameGraphConfig, uint32_t frameResourceIndex) const override;

		void ValidateDescriptorSets(const ShaderDatabase* shaderDatabase, DescriptorDatabase* descriptorDatabase) override;

	private:
		VkImage mSrcImageRef;
		VkImage mDstImageRef;

		VkImageAspectFlags mSrcImageAspectFlags;
		VkImageAspectFlags mDstImageAspectFlags;
	};
}

#include "VulkanCopyImagePass.inl"