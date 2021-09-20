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
	using RenderPassName  = std::string; //Render pass names (chosen by the user)
	using SubresourceId   = std::string; //Subresource ids, unique for the whole frame graph
	using SubresourceName = std::string; //The names assigned to subresources, allowing to bind several subresource ids to a single subresource

public:
	FrameGraphDescription();
	~FrameGraphDescription();

	void AddRenderPass(RenderPassType passType, const std::string_view passName);

	void AssignSubresourceName(const std::string_view subresourceId, const std::string_view subresourceName);
	void AssignBackbufferName(const std::string_view backbufferName);

	std::string_view GetSubresourceName(const SubresourceId& subresourceId) const;
	std::string_view GetBackbufferName() const;

	RenderPassType GetPassType(const RenderPassName& passName) const;

	void GetPassNameList(std::span<const RenderPassName>* outRenderPassNameSpan) const;

private:
	std::vector<RenderPassName> mRenderPassNames;

	std::unordered_map<RenderPassName, RenderPassType> mRenderPassTypes;

	std::unordered_map<SubresourceId, SubresourceName> mSubresourceIdNames;

	SubresourceName mBackbufferName;
};