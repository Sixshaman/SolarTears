#include "CopyImagePass.hpp"

void CopyImagePassBase::RegisterResources(ModernFrameGraphBuilder* frameGraphBuilder, const std::string& passName)
{
	frameGraphBuilder->RegisterSubresource(passName, SrcImageId);
	frameGraphBuilder->RegisterSubresource(passName, DstImageId);
}