void D3D12::CopyImagePass::RegisterResources(std::span<SubresourceMetadataPayload> inoutMetadataPayloads)
{
	assert(inoutMetadataPayloads.size() == (size_t)PassSubresourceId::Count);

	inoutMetadataPayloads[(size_t)PassSubresourceId::SrcImage].State = D3D12_RESOURCE_STATE_COPY_SOURCE;
	inoutMetadataPayloads[(size_t)PassSubresourceId::DstImage].State = D3D12_RESOURCE_STATE_COPY_DEST;
}