#pragma once

#include "../VulkanRenderPass.hpp"
#include "../../../Common/FrameGraph/Passes/CopyImagePass.hpp"

class FrameGraphConfig;

namespace Vulkan
{
	class FrameGraphBuilder;
	class ShaderDatabase;

	class CopyImagePass: public RenderPass, public CopyImagePassBase
	{
	public:
		constexpr inline static VkFormat             GetSubresourceFormat(PassSubresourceId subresourceId);
		constexpr inline static VkImageAspectFlags   GetSubresourceAspect(PassSubresourceId subresourceId);
		constexpr inline static VkPipelineStageFlags GetSubresourceStage(PassSubresourceId subresourceId);
		constexpr inline static VkImageLayout        GetSubresourceLayout(PassSubresourceId subresourceId);
		constexpr inline static VkImageUsageFlags    GetSubresourceUsage(PassSubresourceId subresourceId);
		constexpr inline static VkAccessFlags        GetSubresourceAccess(PassSubresourceId subresourceId);

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