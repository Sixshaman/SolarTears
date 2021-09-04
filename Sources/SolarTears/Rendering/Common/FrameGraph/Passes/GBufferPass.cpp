#include "GBufferPass.hpp"

void GBufferPassBase::RegisterResources(ModernFrameGraphBuilder* frameGraphBuilder, const std::string& passName)
{
	frameGraphBuilder->RegisterWriteSubresource(passName, ColorBufferImageId);
}