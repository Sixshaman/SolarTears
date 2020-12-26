constexpr size_t VulkanCBindings::RenderableSceneBuilder::GetVertexSize()
{
	return sizeof(RenderableSceneVertex);
}

constexpr VkFormat VulkanCBindings::RenderableSceneBuilder::GetVertexPositionFormat()
{
	return VulkanUtils::FormatForVectorType<decltype(RenderableSceneVertex::Position)>;
}

constexpr VkFormat VulkanCBindings::RenderableSceneBuilder::GetVertexNormalFormat()
{
	return VulkanUtils::FormatForVectorType<decltype(RenderableSceneVertex::Normal)>;
}

constexpr VkFormat VulkanCBindings::RenderableSceneBuilder::GetVertexTexcoordFormat()
{
	return VulkanUtils::FormatForVectorType<decltype(RenderableSceneVertex::Texcoord)>;
}

constexpr uint32_t VulkanCBindings::RenderableSceneBuilder::GetVertexPositionOffset()
{
	return offsetof(RenderableSceneVertex, Position);
}

constexpr uint32_t VulkanCBindings::RenderableSceneBuilder::GetVertexNormalOffset()
{
	return offsetof(RenderableSceneVertex, Normal);
}

constexpr uint32_t VulkanCBindings::RenderableSceneBuilder::GetVertexTexcoordOffset()
{
	return offsetof(RenderableSceneVertex, Texcoord);
}