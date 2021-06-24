#pragma once

#include "../ModernFrameGraphMisc.hpp"

class GBufferPassBase
{
public:
	static constexpr RenderPassClass PassClass = RenderPassClass::Graphics;
	static constexpr RenderPassType  PassType  = RenderPassType::GBufferGenerate;


	static constexpr std::string_view ColorBufferImageId = "GBufferPass-ColorBufferImage";
};