#pragma once

#include "../ModernFrameGraphMisc.hpp"

class CopyImagePassBase
{
public:
	static constexpr RenderPassClass PassClass = RenderPassClass::Transfer;
	static constexpr RenderPassType  PassType  = RenderPassType::CopyImage;


	static constexpr std::string_view SrcImageId = "CopyImagePass-SrcImage";
	static constexpr std::string_view DstImageId = "CopyImagePass-DstImage";
};