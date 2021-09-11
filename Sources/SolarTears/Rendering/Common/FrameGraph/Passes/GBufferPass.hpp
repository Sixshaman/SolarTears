#pragma once

#include "../ModernFrameGraphMisc.hpp"
#include "../ModernFrameGraphBuilder.hpp"

class GBufferPassBase
{
public:
	static constexpr RenderPassClass PassClass = RenderPassClass::Graphics;
	static constexpr RenderPassType  PassType = RenderPassType::GBufferGenerate;

public:
	static constexpr std::string_view ColorBufferImageId = "GBufferColorImage";

public:
	static void RegisterResources(ModernFrameGraphBuilder* frameGraphBuilder, const std::string& passName);
};