constexpr size_t Vulkan::RenderableSceneBuilder::GetVertexSize()
{
	return sizeof(RenderableSceneVertex);
}

constexpr VkFormat Vulkan::RenderableSceneBuilder::GetVertexPositionFormat()
{
	return VulkanUtils::FormatForVectorType<decltype(RenderableSceneVertex::Position)>;
}

constexpr VkFormat Vulkan::RenderableSceneBuilder::GetVertexNormalFormat()
{
	return VulkanUtils::FormatForVectorType<decltype(RenderableSceneVertex::Normal)>;
}

constexpr VkFormat Vulkan::RenderableSceneBuilder::GetVertexTexcoordFormat()
{
	return VulkanUtils::FormatForVectorType<decltype(RenderableSceneVertex::Texcoord)>;
}

constexpr uint32_t Vulkan::RenderableSceneBuilder::GetVertexPositionOffset()
{
	return offsetof(RenderableSceneVertex, Position);
}

constexpr uint32_t Vulkan::RenderableSceneBuilder::GetVertexNormalOffset()
{
	return offsetof(RenderableSceneVertex, Normal);
}

constexpr uint32_t Vulkan::RenderableSceneBuilder::GetVertexTexcoordOffset()
{
	return offsetof(RenderableSceneVertex, Texcoord);
}