#include "FrameGraphDescription.hpp"
#include <cassert>

FrameGraphDescription::FrameGraphDescription()
{
}

FrameGraphDescription::~FrameGraphDescription()
{
}

void FrameGraphDescription::AddRenderPass(RenderPassType passType, const std::string_view passName)
{
	FrameGraphDescription::RenderPassName renderPassName(passName);
	assert(!mRenderPassTypes.contains(renderPassName));

	mRenderPassNames.push_back(renderPassName);
	mRenderPassTypes[renderPassName] = passType;
}

void FrameGraphDescription::AssignSubresourceName(const std::string_view subresourceId, const std::string_view subresourceName)
{
	FrameGraphDescription::SubresourceId   subresourceIdStr(subresourceName);
	FrameGraphDescription::SubresourceName subresourceNameStr(subresourceName);

	assert(!mSubresourceIdNames.contains(subresourceNameStr));
	mSubresourceIdNames[subresourceNameStr] = subresourceIdStr;
}

void FrameGraphDescription::AssignBackbufferName(const std::string_view backbufferName)
{
	mBackbufferName = backbufferName;

	AssignSubresourceName(BackbufferPresentPassId, mBackbufferName);
}