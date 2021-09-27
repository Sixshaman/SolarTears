inline void Vulkan::GBufferPass::RegisterSubresources(std::span<SubresourceMetadataPayload> inoutMetadataPayloads)
{
	assert(inoutMetadataPayloads.size() == (size_t)PassSubresourceId::Count);

	inoutMetadataPayloads[(size_t)PassSubresourceId::ColorBufferImage].Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	inoutMetadataPayloads[(size_t)PassSubresourceId::ColorBufferImage].Layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	inoutMetadataPayloads[(size_t)PassSubresourceId::ColorBufferImage].Usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	inoutMetadataPayloads[(size_t)PassSubresourceId::ColorBufferImage].Stage  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	inoutMetadataPayloads[(size_t)PassSubresourceId::ColorBufferImage].Access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	inoutMetadataPayloads[(size_t)PassSubresourceId::ColorBufferImage].Flags  = (TextureFlagAutoBeforeBarrier | TextureFlagAutoBeforeBarrier);
}

inline void Vulkan::GBufferPass::RegisterShaders(ShaderDatabase* shaderDatabase)
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