constexpr inline VkFormat Vulkan::GBufferPass::GetSubresourceFormat(PassSubresourceId subresourceId)
{
	switch(subresourceId)
	{
		case PassSubresourceId::ColorBufferImage: return VK_FORMAT_B8G8R8A8_UNORM; //TODO: maybe parametrize the format somehow?
	}

	assert(false); 
	return VK_FORMAT_UNDEFINED;
}

VkImageAspectFlags Vulkan::GBufferPass::GetSubresourceAspect(PassSubresourceId subresourceId)
{
	switch(subresourceId)
	{
		case PassSubresourceId::ColorBufferImage: return VK_IMAGE_ASPECT_COLOR_BIT;
	}

	assert(false);
	return 0;
}

VkPipelineStageFlags Vulkan::GBufferPass::GetSubresourceStage(PassSubresourceId subresourceId)
{
	switch(subresourceId)
	{
		case PassSubresourceId::ColorBufferImage: return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	}

	assert(false);
	return 0;
}

VkImageLayout Vulkan::GBufferPass::GetSubresourceLayout(PassSubresourceId subresourceId)
{
	switch(subresourceId)
	{
		case PassSubresourceId::ColorBufferImage: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	assert(false);
	return VK_IMAGE_LAYOUT_UNDEFINED;
}

VkImageUsageFlags Vulkan::GBufferPass::GetSubresourceUsage(PassSubresourceId subresourceId)
{
	switch(subresourceId)
	{
		case PassSubresourceId::ColorBufferImage: return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}

	assert(false);
	return (VkImageUsageFlagBits)0;
}

VkAccessFlags Vulkan::GBufferPass::GetSubresourceAccess(PassSubresourceId subresourceId)
{
	switch(subresourceId)
	{
		case PassSubresourceId::ColorBufferImage: return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	}

	assert(false);
	return 0;
}

void Vulkan::GBufferPass::RegisterShaders(ShaderDatabase* shaderDatabase)
{
	//Load shaders in advance to process all bindings common for all passes (scene, samplers)
	const std::wstring shaderFolder = Utils::GetMainDirectory() + L"Shaders/Vulkan/GBuffer/";
	
	std::wstring staticShaderFilename          = shaderFolder + L"GBufferDrawStatic.vert.spv";
	std::wstring staticInstancedShaderFilename = shaderFolder + L"GBufferDrawStaticInstanced.vert.spv";
	std::wstring rigidShaderFilename           = shaderFolder + L"GBufferDrawRigid.vert.spv";
	std::wstring fragmentShaderFilename        = shaderFolder + L"GBufferDraw.frag.spv";

	std::array staticShaders          = {staticShaderFilename,          fragmentShaderFilename};
	std::array staticInstancedShaders = {staticInstancedShaderFilename, fragmentShaderFilename};
	std::array rigidShaders           = {rigidShaderFilename,           fragmentShaderFilename};

	shaderDatabase->RegisterShaderGroup(StaticShaderGroup,          staticShaders);
	shaderDatabase->RegisterShaderGroup(StaticInstancedShaderGroup, staticInstancedShaders);
	shaderDatabase->RegisterShaderGroup(RigidShaderGroup,           rigidShaders);
}