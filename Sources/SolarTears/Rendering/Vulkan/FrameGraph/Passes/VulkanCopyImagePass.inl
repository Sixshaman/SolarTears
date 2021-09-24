constexpr inline VkFormat Vulkan::CopyImagePass::GetSubresourceFormat(PassSubresourceId subresourceId)
{
	switch(subresourceId)
	{
		case PassSubresourceId::SrcImage: return VK_FORMAT_UNDEFINED;
		case PassSubresourceId::DstImage: return VK_FORMAT_UNDEFINED;
	}

	assert(false); 
	return VK_FORMAT_UNDEFINED;
}

VkImageAspectFlags Vulkan::CopyImagePass::GetSubresourceAspect(PassSubresourceId subresourceId)
{
	switch(subresourceId)
	{
		case PassSubresourceId::SrcImage: return 0;
		case PassSubresourceId::DstImage: return 0;
	}

	assert(false);
	return 0;
}

VkPipelineStageFlags Vulkan::CopyImagePass::GetSubresourceStage(PassSubresourceId subresourceId)
{
	switch(subresourceId)
	{
		case PassSubresourceId::SrcImage: return VK_PIPELINE_STAGE_TRANSFER_BIT;
		case PassSubresourceId::DstImage: return VK_PIPELINE_STAGE_TRANSFER_BIT;
	}

	assert(false);
	return 0;
}

VkImageLayout Vulkan::CopyImagePass::GetSubresourceLayout(PassSubresourceId subresourceId)
{
	switch(subresourceId)
	{
		case PassSubresourceId::SrcImage: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		case PassSubresourceId::DstImage: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	}

	assert(false);
	return VK_IMAGE_LAYOUT_UNDEFINED;
}

VkImageUsageFlags Vulkan::CopyImagePass::GetSubresourceUsage(PassSubresourceId subresourceId)
{
	switch(subresourceId)
	{
		case PassSubresourceId::SrcImage: return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		case PassSubresourceId::DstImage: return VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	assert(false);
	return (VkImageUsageFlagBits)0;
}

VkAccessFlags Vulkan::CopyImagePass::GetSubresourceAccess(PassSubresourceId subresourceId)
{
	switch(subresourceId)
	{
		case PassSubresourceId::SrcImage: return VK_ACCESS_TRANSFER_READ_BIT;
		case PassSubresourceId::DstImage: return VK_ACCESS_TRANSFER_WRITE_BIT;
	}

	assert(false);
	return 0;
}

void Vulkan::CopyImagePass::RegisterShaders([[maybe_unused]] ShaderDatabase* shaderDatabase)
{
	//Nothing
}