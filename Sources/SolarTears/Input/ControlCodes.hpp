#pragma once

#include <cstdint>

enum class ControlCode : uint8_t
{
	MoveForward,
	MoveBack,
	MoveLeft,
	MoveRight,

	Count,
	Nothing = Count,
};

static constexpr uint8_t MaxControlCodes = 32;

static_assert((uint8_t)ControlCode::Count <= MaxControlCodes);