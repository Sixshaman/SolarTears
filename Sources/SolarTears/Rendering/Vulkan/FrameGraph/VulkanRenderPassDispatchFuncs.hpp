#pragma once

#include "../../Common/FrameGraph/RenderPassDispatchFuncs.hpp"
#include "Passes/VulkanGBufferPass.hpp"
#include "Passes/VulkanCopyImagePass.hpp"
#include <memory>
#include <vulkan/vulkan.h>

namespace Vulkan
{
	class FrameGraphBuilder;

	template<typename Pass>
	std::unique_ptr<RenderPass> MakeUniquePass(const FrameGraphBuilder* builder, const std::string& passName, uint32_t frame)
	{
		return std::make_unique<Pass>(device, builder, passName, frame);
	};

	std::unique_ptr<RenderPass> MakeUniquePass(RenderPassType passType, const FrameGraphBuilder* builder, const std::string& passName, uint32_t frame)
	{                                                                                                                              
		switch(passType)                                                                                                           
		{                                                                                                                          
			case RenderPassType::GBufferGenerate: return MakeUniquePass<GBufferPass>(builder, passName, frame);
			case RenderPassType::CopyImage:       return MakeUniquePass<CopyImagePass>(builder, passName, frame);
		}                                                                                                                          
																																   
		assert(false);                                                                                                             
		return nullptr;                                                                                                      
	}

	template<typename Pass>
	inline static void RegisterPassShaders(ShaderDatabase* shaderDatabase)
	{

	}

	DEFINE_GET_PASS_SUBRESOURCE_INFO_TEMPLATE(std::string_view, StringId)
	DEFINE_GET_PASS_SUBRESOURCE_INFO(std::string_view, StringId, "")

	DEFINE_GET_PASS_SUBRESOURCE_INFO_TEMPLATE(VkFormat, Format)
	DEFINE_GET_PASS_SUBRESOURCE_INFO(VkFormat, Format, VK_FORMAT_UNDEFINED)

	DEFINE_GET_PASS_SUBRESOURCE_INFO_TEMPLATE(VkImageAspectFlags, Aspect)
	DEFINE_GET_PASS_SUBRESOURCE_INFO(VkImageAspectFlags, Aspect, 0)

	DEFINE_GET_PASS_SUBRESOURCE_INFO_TEMPLATE(VkPipelineStageFlags, Stage)
	DEFINE_GET_PASS_SUBRESOURCE_INFO(VkPipelineStageFlags, Stage, 0)

	DEFINE_GET_PASS_SUBRESOURCE_INFO_TEMPLATE(VkImageLayout, Layout)
	DEFINE_GET_PASS_SUBRESOURCE_INFO(VkImageLayout, Layout, VK_IMAGE_LAYOUT_UNDEFINED)

	DEFINE_GET_PASS_SUBRESOURCE_INFO_TEMPLATE(VkImageUsageFlags, Usage)
	DEFINE_GET_PASS_SUBRESOURCE_INFO(VkImageUsageFlags, Usage, 0)

	DEFINE_GET_PASS_SUBRESOURCE_INFO_TEMPLATE(VkAccessFlags, Access)
	DEFINE_GET_PASS_SUBRESOURCE_INFO(VkAccessFlags, Access, VK_FORMAT_UNDEFINED)
}