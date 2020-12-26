#include "VulkanCRenderPass.hpp"

VulkanCBindings::RenderPass::RenderPass(VkDevice device, RenderPassType type): mDeviceRef(device), mRenderPassType(type)
{
}

VulkanCBindings::RenderPass::~RenderPass()
{
}

VulkanCBindings::RenderPassType VulkanCBindings::RenderPass::GetRenderPassType() const
{
	return mRenderPassType;
}