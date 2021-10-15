void D3D12::GBufferPass::RegisterSubresources(std::span<SubresourceMetadataPayload> inoutMetadataPayloads)
{
	assert(inoutMetadataPayloads.size() == (size_t)PassSubresourceId::Count);

	inoutMetadataPayloads[(size_t)PassSubresourceId::ColorBufferImage].State = D3D12_RESOURCE_STATE_RENDER_TARGET;
}

inline bool D3D12::GBufferPass::PropagateSubresourceInfos([[maybe_unused]] std::span<SubresourceMetadataPayload> inoutMetadataPayloads)
{
	return false; //No horizontal propagation happens here
}