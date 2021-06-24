#pragma once

#include <cstdint>

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
	CopyImage
};