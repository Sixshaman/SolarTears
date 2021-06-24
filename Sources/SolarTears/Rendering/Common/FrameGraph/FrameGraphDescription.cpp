#include "FrameGraphDescription.hpp"

FrameGraphDescription::FrameGraphDescription()
{
}

FrameGraphDescription::~FrameGraphDescription()
{
}

void FrameGraphDescription::AddRenderPass(RenderPassType passType, const std::string_view passName)
{
	FrameGraphDescription::RenderPassName renderPassName(passName);
	mRenderPassNames.push_back(renderPassName);

	mRenderPassTypes[renderPassName] = passType;
}

void FrameGraphDescription::AssignSubresourceName(const std::string_view passName, const std::string_view subresourceId, const std::string_view subresourceName)
{
	FrameGraphDescription::RenderPassName  passNameStr(passName);
	FrameGraphDescription::SubresourceId   subresourceIdStr(subresourceId);
	FrameGraphDescription::SubresourceName subresourceNameStr(subresourceName);

	mRenderPassesSubresourceNameIds[passNameStr][subresourceNameStr] = subresourceIdStr;
}

void FrameGraphDescription::AssignBackbufferName(const std::string_view backbufferName)
{
	mBackbufferName = backbufferName;

	AssignSubresourceName(PresentPassName, BackbufferPresentPassId, mBackbufferName);
}