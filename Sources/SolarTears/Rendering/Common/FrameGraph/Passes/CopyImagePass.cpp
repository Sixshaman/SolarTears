#include "CopyImagePass.hpp"

void CopyImagePassBase::OnAdd(ModernFrameGraphBuilder* frameGraphBuilder, const std::string& passName)
{
	frameGraphBuilder->RegisterReadSubresource(passName,  SrcImageId);
	frameGraphBuilder->RegisterWriteSubresource(passName, DstImageId);
}