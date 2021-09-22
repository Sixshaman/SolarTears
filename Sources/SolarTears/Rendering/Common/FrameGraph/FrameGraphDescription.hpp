#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <span>
#include "ModernFrameGraphMisc.hpp"

class FrameGraphDescription
{
public:
	constexpr static std::string_view PresentPassName         = "SPECIAL_PRESENT_ACQUIRE_PASS";
	constexpr static std::string_view BackbufferPresentPassId = "SpecialPresentAcquirePass-Backbuffer";

public:
	using RenderPassName = std::string; //Render pass names (chosen by the user)
	using ResourceName   = std::string; //The names defining a unique resource. Several subresources with the same ResourceName define a single resource
	using SubresourceId  = std::string; //Subresource ids, unique for all render pass types

private:
	struct SubresourceNamingInfo
	{
		RenderPassName PassName;
		SubresourceId  PassSubresourceId;
		ResourceName   PassSubresourceName;
	};

public:
	FrameGraphDescription();
	~FrameGraphDescription();

	void AddRenderPass(RenderPassType passType, const std::string_view passName);

	void AssignSubresourceName(const std::string_view passName, const std::string_view subresourceId, const std::string_view subresourceName);
	void AssignBackbufferName(const std::string_view backbufferName);

private:
	std::unordered_map<RenderPassName, RenderPassType> mRenderPassTypes;

	std::vector<SubresourceNamingInfo> mSubresourceNames;
};