#pragma once

#include "../ModernFrameGraphMisc.hpp"
#include "../ModernFrameGraphBuilder.hpp"

class CopyImagePassBase
{
public:
	static constexpr RenderPassClass PassClass = RenderPassClass::Transfer;
	static constexpr RenderPassType  PassType  = RenderPassType::CopyImage;

public:
	static constexpr std::string_view SrcImageId = "CopyImagePass-SrcImage";
	static constexpr std::string_view DstImageId = "CopyImagePass-DstImage";

public:
	static void OnAdd(ModernFrameGraphBuilder* frameGraphBuilder, const std::string& passName);
};