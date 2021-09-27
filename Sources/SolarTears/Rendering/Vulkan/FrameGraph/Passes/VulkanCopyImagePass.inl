inline void Vulkan::CopyImagePass::RegisterSubresources(std::span<SubresourceMetadataPayload> inoutMetadataPayloads)
{
	assert(inoutMetadataPayloads.size() == (size_t)PassSubresourceId::Count);

	uint32_t srcImageIndex = (size_t)PassSubresourceId::SrcImage;
	uint32_t dstImageIndex = (size_t)PassSubresourceId::DstImage;

	inoutMetadataPayloads[srcImageIndex].Layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	inoutMetadataPayloads[srcImageIndex].Usage  = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	inoutMetadataPayloads[srcImageIndex].Stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
	inoutMetadataPayloads[srcImageIndex].Access = VK_ACCESS_TRANSFER_READ_BIT;

	inoutMetadataPayloads[dstImageIndex].Layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	inoutMetadataPayloads[dstImageIndex].Usage  = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	inoutMetadataPayloads[dstImageIndex].Stage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
	inoutMetadataPayloads[dstImageIndex].Access = VK_ACCESS_TRANSFER_WRITE_BIT;
}

void Vulkan::CopyImagePass::RegisterShaders([[maybe_unused]] ShaderDatabase* shaderDatabase)
{
	//Nothing
}

inline bool Vulkan::CopyImagePass::PropagateSubresourceInfos(std::span<SubresourceMetadataPayload> inoutMetadataPayloads)
{
	uint32_t srcImageIndex = (size_t)PassSubresourceId::SrcImage;
	uint32_t dstImageIndex = (size_t)PassSubresourceId::DstImage;

	bool propagationHappened = false;

	if(inoutMetadataPayloads[srcImageIndex].Format == VK_FORMAT_UNDEFINED && inoutMetadataPayloads[dstImageIndex].Format != VK_FORMAT_UNDEFINED)
	{
		inoutMetadataPayloads[srcImageIndex].Format = inoutMetadataPayloads[dstImageIndex].Format;
		propagationHappened = true;
	}

	if(inoutMetadataPayloads[dstImageIndex].Format == VK_FORMAT_UNDEFINED && inoutMetadataPayloads[srcImageIndex].Format != VK_FORMAT_UNDEFINED)
	{
		inoutMetadataPayloads[dstImageIndex].Format = inoutMetadataPayloads[srcImageIndex].Format;
		propagationHappened = true;
	}

	return propagationHappened;
}
