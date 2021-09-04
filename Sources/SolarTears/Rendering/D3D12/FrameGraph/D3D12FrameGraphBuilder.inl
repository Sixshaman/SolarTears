template<typename Pass>
inline void D3D12::FrameGraphBuilder::AddPassToTable()
{
	mRenderPassClassTable[Pass::PassType] = Pass::PassClass;

	mPassAddFuncTable[Pass::PassType]    = Pass::RegisterResources;
	mPassCreateFuncTable[Pass::PassType] = [](ID3D12Device8* device, const FrameGraphBuilder* builder, const std::string& passName, uint32_t frame) -> std::unique_ptr<RenderPass> {return std::make_unique<Pass>(device, builder, passName, frame);};
}