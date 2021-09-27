void D3D12::CopyImagePass::RegisterSubesources(std::span<SubresourceMetadataPayload> inoutMetadataPayloads)
{
	assert(inoutMetadataPayloads.size() == (size_t)PassSubresourceId::Count);

	inoutMetadataPayloads[(size_t)PassSubresourceId::SrcImage].State = D3D12_RESOURCE_STATE_COPY_SOURCE;
	inoutMetadataPayloads[(size_t)PassSubresourceId::DstImage].State = D3D12_RESOURCE_STATE_COPY_DEST;
}

inline bool D3D12::CopyImagePass::PropagateSubresourceInfos(std::span<SubresourceMetadataPayload> inoutMetadataPayloads)
{
	uint32_t srcImageIndex = (size_t)PassSubresourceId::SrcImage;
	uint32_t dstImageIndex = (size_t)PassSubresourceId::DstImage;

	bool propagationHappened = false;

	if(inoutMetadataPayloads[srcImageIndex].Format == DXGI_FORMAT_UNKNOWN && inoutMetadataPayloads[dstImageIndex].Format != DXGI_FORMAT_UNKNOWN)
	{
		inoutMetadataPayloads[srcImageIndex].Format = inoutMetadataPayloads[dstImageIndex].Format;
		propagationHappened = true;
	}

	if(inoutMetadataPayloads[dstImageIndex].Format == DXGI_FORMAT_UNKNOWN && inoutMetadataPayloads[srcImageIndex].Format != DXGI_FORMAT_UNKNOWN)
	{
		inoutMetadataPayloads[dstImageIndex].Format = inoutMetadataPayloads[srcImageIndex].Format;
		propagationHappened = true;
	}

	return propagationHappened;
}