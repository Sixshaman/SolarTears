constexpr size_t D3D12::RenderableSceneBuilder::GetVertexSize()
{
	return sizeof(RenderableSceneVertex);
}

constexpr DXGI_FORMAT D3D12::RenderableSceneBuilder::GetVertexPositionFormat()
{
	return D3D12Utils::FormatForVectorType<decltype(RenderableSceneVertex::Position)>;
}

constexpr DXGI_FORMAT D3D12::RenderableSceneBuilder::GetVertexNormalFormat()
{
	return D3D12Utils::FormatForVectorType<decltype(RenderableSceneVertex::Normal)>;
}

constexpr DXGI_FORMAT D3D12::RenderableSceneBuilder::GetVertexTexcoordFormat()
{
	return D3D12Utils::FormatForVectorType<decltype(RenderableSceneVertex::Texcoord)>;
}

constexpr uint32_t D3D12::RenderableSceneBuilder::GetVertexPositionOffset()
{
	return offsetof(RenderableSceneVertex, Position);
}

constexpr uint32_t D3D12::RenderableSceneBuilder::GetVertexNormalOffset()
{
	return offsetof(RenderableSceneVertex, Normal);
}

constexpr uint32_t D3D12::RenderableSceneBuilder::GetVertexTexcoordOffset()
{
	return offsetof(RenderableSceneVertex, Texcoord);
}