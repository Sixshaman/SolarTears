#pragma once

#include <cstdint>
#include <string_view>
#include <string>

using RenderPassName = std::string; //Render pass names (chosen by the user)
using ResourceName   = std::string; //The names defining a unique resource. Several subresources with the same ResourceName define a single resource
using SubresourceId  = std::string; //Subresource ids, unique for all render pass types

constexpr static std::string_view PresentPassName         = "SPECIAL_PRESENT_ACQUIRE_PASS";
constexpr static std::string_view PresentPassBackbufferId = "SpecialPresentAcquirePass-Backbuffer";

enum class RenderPassClass: uint32_t
{
	Graphics = 0,
	Compute,
	Transfer,
	Present,

	Count
};

enum class RenderPassType: uint32_t
{
	GBufferGenerate,
	GBufferDraw,
	CopyImage,

	None = 0xffffffff
};