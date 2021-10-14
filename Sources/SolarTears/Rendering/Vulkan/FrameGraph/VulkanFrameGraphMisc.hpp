#pragma once

namespace Vulkan
{
	constexpr uint32_t TextureFlagAutoBeforeBarrier = 0x01; //A before-barrier is handled by render pass itself
	constexpr uint32_t TextureFlagAutoAfterBarrier  = 0x02; //An after-barrier is handled by render pass itself

	//Additional Vulkan-specific data for each subresource metadata node
	struct SubresourceMetadataPayload
	{
		SubresourceMetadataPayload()
		{
			Format = VK_FORMAT_UNDEFINED;
			Aspect = 0;
			Layout = VK_IMAGE_LAYOUT_UNDEFINED;
			Usage  = 0;
			Stage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			Access = 0;
			Flags  = 0;
		}

		VkFormat           Format;
		VkImageAspectFlags Aspect;

		VkImageLayout        Layout;
		VkImageUsageFlags    Usage;
		VkPipelineStageFlags Stage;
		VkAccessFlags        Access;

		uint32_t Flags;
	};

	struct DescriptorSetBindRange
	{
		uint32_t Begin;
		uint32_t End;
		uint32_t BindPoint;
	};
}