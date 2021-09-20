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

std::string_view FrameGraphDescription::GetSubresourceName(const SubresourceId& subresourceId) const
{
	return mSubresourceIdNames.at(subresourceId);
}

std::string_view FrameGraphDescription::GetBackbufferName() const
{
	return mBackbufferName;
}

RenderPassType FrameGraphDescription::GetPassType(const RenderPassName& passName) const
{
	return mRenderPassTypes.at(passName);
}

void FrameGraphDescription::GetPassNameList(std::span<const RenderPassName>* outRenderPassNameSpan) const
{
	*outRenderPassNameSpan = {mRenderPassNames.cbegin(), mRenderPassNames.cend()};
}
