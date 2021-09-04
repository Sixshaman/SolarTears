#include "CopyImagePass.hpp"

void CopyImagePassBase::RegisterResources(ModernFrameGraphBuilder* frameGraphBuilder, const std::string& passName)
{
	frameGraphBuilder->RegisterReadSubresource(passName,  SrcImageId);
	frameGraphBuilder->RegisterWriteSubresource(passName, DstImageId);
}