inline void Vulkan::CopyImagePass::RegisterSubresources(std::span<SubresourceMetadataPayload> inoutMetadataPayloads)
{
	assert(inoutMetadataPayloads.size() == (size_t)PassSubresourceId::Count);

	inoutMetadataPayloads[(size_t)PassSubresourceId::SrcImage].Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	inoutMetadataPayloads[(size_t)PassSubresourceId::SrcImage].Usage  = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	inoutMetadataPayloads[(size_t)PassSubresourceId::SrcImage].Stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
	inoutMetadataPayloads[(size_t)PassSubresourceId::SrcImage].Access = VK_ACCESS_TRANSFER_READ_BIT;

	inoutMetadataPayloads[(size_t)PassSubresourceId::DstImage].Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	inoutMetadataPayloads[(size_t)PassSubresourceId::DstImage].Usage  = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	inoutMetadataPayloads[(size_t)PassSubresourceId::DstImage].Stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
	inoutMetadataPayloads[(size_t)PassSubresourceId::DstImage].Access = VK_ACCESS_TRANSFER_WRITE_BIT;
}

void Vulkan::CopyImagePass::RegisterShaders([[maybe_unused]] ShaderDatabase* shaderDatabase)
{
	//Nothing
}