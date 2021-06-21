#pragma once

#include <cstdint>

enum class RenderPassType: uint32_t
{
	Graphics = 0,
	Compute,
	Transfer,
	Present,

	Count
};