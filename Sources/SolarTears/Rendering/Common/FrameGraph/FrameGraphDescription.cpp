#include "FrameGraphDescription.hpp"
#include "Passes/PresentPass.hpp"
#include <cassert>

FrameGraphDescription::FrameGraphDescription()
{
}

FrameGraphDescription::~FrameGraphDescription()
{
}

void FrameGraphDescription::AddRenderPass(RenderPassType passType, const std::string_view passName)
{
	RenderPassName renderPassName(passName);
	assert(!mRenderPassTypes.contains(renderPassName));
	mRenderPassTypes[renderPassName] = passType;
}

void FrameGraphDescription::AssignSubresourceName(const std::string_view passName, const std::string_view subresourceId, const std::string_view subresourceName)
{
	RenderPassName renderPassName(passName);
	assert(mRenderPassTypes.contains(renderPassName));

	mSubresourceNames.push_back(SubresourceNamingInfo
	{
		.PassName            = renderPassName,
		.PassSubresourceId   = SubresourceId(subresourceId),
		.PassSubresourceName = ResourceName(subresourceName)
	});
}

void FrameGraphDescription::AssignBackbufferName(const std::string_view backbufferName)
{
	mBackbufferName = backbufferName;
}