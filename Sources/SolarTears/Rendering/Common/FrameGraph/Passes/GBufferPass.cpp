#include "GBufferPass.hpp"

void GBufferPassBase::OnAdd(ModernFrameGraphBuilder* frameGraphBuilder, const std::string& passName)
{
	frameGraphBuilder->RegisterWriteSubresource(passName, ColorBufferImageId);
}