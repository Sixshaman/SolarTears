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
	mRenderPassTypes[renderPassName] = passType;
}

void FrameGraphDescription::AssignSubresourceName(const std::string_view passName, const std::string_view subresourceId, const std::string_view subresourceName)
{
	FrameGraphDescription::RenderPassName renderPassName(passName);
	assert(mRenderPassTypes.contains(renderPassName));

	mSubresourceNames.push_back(SubresourceNamingInfo
	{
		.PassName            = renderPassName,
		.PassSubresourceId   = FrameGraphDescription::SubresourceId(subresourceId),
		.PassSubresourceName = FrameGraphDescription::ResourceName(subresourceName)
	});
}

void FrameGraphDescription::AssignBackbufferName(const std::string_view backbufferName)
{
	mSubresourceNames.push_back(SubresourceNamingInfo
	{
		.PassName            = FrameGraphDescription::RenderPassName(PresentPassName),
		.PassSubresourceId   = FrameGraphDescription::SubresourceId(BackbufferPresentPassId),
		.PassSubresourceName = FrameGraphDescription::ResourceName(backbufferName)
	});
}