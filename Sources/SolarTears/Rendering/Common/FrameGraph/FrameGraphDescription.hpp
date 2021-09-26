#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <span>
#include "ModernFrameGraphMisc.hpp"

struct SubresourceNamingInfo
{
	RenderPassName PassName;
	SubresourceId  PassSubresourceId;
	ResourceName   PassSubresourceName;
};

struct FrameGraphDescription
{
	std::unordered_map<RenderPassName, RenderPassType> mRenderPassTypes;
	std::vector<SubresourceNamingInfo>                 mSubresourceNames;

	//Helper functions
	void AddRenderPass(RenderPassType passType, const std::string_view passName);
	void AssignSubresourceName(const std::string_view passName, const std::string_view subresourceId, const std::string_view subresourceName);
	void AssignBackbufferName(const std::string_view backbufferName);
};