#pragma once

#include "../ModernFrameGraphMisc.hpp"
#include "../ModernFrameGraphBuilder.hpp"

class GBufferPassBase
{
public:
	static constexpr RenderPassClass PassClass = RenderPassClass::Graphics;
	static constexpr RenderPassType  PassType = RenderPassType::GBufferGenerate;

public:
	static constexpr std::string_view ColorBufferImageId = "ColorBufferImage";

public:
	static void OnAdd(ModernFrameGraphBuilder* frameGraphBuilder, const std::string& passName);
};