void D3D12::GBufferPass::RegisterResources(std::span<SubresourceMetadataPayload> inoutMetadataPayloads)
{
	assert(inoutMetadataPayloads.size() == (size_t)PassSubresourceId::Count);

	inoutMetadataPayloads[(size_t)PassSubresourceId::ColorBufferImage].State = D3D12_RESOURCE_STATE_RENDER_TARGET;
}