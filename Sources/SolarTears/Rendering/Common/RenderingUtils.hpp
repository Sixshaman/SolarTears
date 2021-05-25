#pragma once

#include <cstdint>

namespace Utils
{
	//3 frames in flight for modern scenes is good enough
	constexpr uint32_t InFlightFrameCount = 3;

	uint64_t AlignMemory(uint64_t value, uint64_t alignment);
}