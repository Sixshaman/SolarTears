template<typename Pass>
inline void Vulkan::FrameGraphBuilder::AddPassToTable()
{
	mRenderPassClassTable[Pass::PassType] = Pass::PassClass;

	mPassAddFuncTable[Pass::PassType]    = Pass::OnAdd;
	mPassCreateFuncTable[Pass::PassType] = [](VkDevice device, const FrameGraphBuilder* builder, const std::string& passName, uint32_t frame) -> std::unique_ptr<RenderPass> {return std::make_unique<Pass>(device, builder, passName, frame);};
}