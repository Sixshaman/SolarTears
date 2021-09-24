template<typename Pass>
inline void Vulkan::FrameGraphBuilder::AddPassToTable()
{
	mRenderPassClassTable[Pass::PassType] = Pass::PassClass;

	mPassAddFuncTable[Pass::PassType]    = Pass::RegisterResources;
	mPassCreateFuncTable[Pass::PassType] = Pass::RegisterShaders;


}