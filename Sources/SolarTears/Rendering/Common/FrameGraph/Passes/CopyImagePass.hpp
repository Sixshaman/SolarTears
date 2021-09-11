#pragma once

#include "../ModernFrameGraphMisc.hpp"
#include "../ModernFrameGraphBuilder.hpp"

class CopyImagePassBase
{
public:
	static constexpr RenderPassClass PassClass = RenderPassClass::Transfer;
	static constexpr RenderPassType  PassType  = RenderPassType::CopyImage;

public: 
	static constexpr std::string_view SrcImageId = "CopySrcImage";
	static constexpr std::string_view DstImageId = "CopyDstImage";

public:
	static void RegisterResources(ModernFrameGraphBuilder* frameGraphBuilder, const std::string& passName);
};