#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "ModernFrameGraphMisc.hpp"

class FrameGraphDescription
{
	friend class ModernFrameGraphBuilder;

	constexpr static std::string_view PresentPassName         = "SPECIAL_PRESENT_ACQUIRE_PASS";
	constexpr static std::string_view BackbufferPresentPassId = "SpecialPresentAcquirePass-Backbuffer";

public:
	using RenderPassName  = std::string;
	using SubresourceId   = std::string;
	using SubresourceName = std::string;

public:
	FrameGraphDescription();
	~FrameGraphDescription();

	void AddRenderPass(RenderPassType passType, const std::string_view passName);

	void AssignSubresourceName(const std::string_view passName, const std::string_view subresourceId, const std::string_view subresourceName);
	void AssignBackbufferName(const std::string_view backbufferName);

private:
	std::vector<RenderPassName> mRenderPassNames;

	std::unordered_map<RenderPassName, RenderPassType> mRenderPassTypes;

	std::unordered_map<RenderPassName, std::unordered_map<SubresourceName, SubresourceId>> mRenderPassesSubresourceNameIds;

	SubresourceName mBackbufferName;
};