#pragma once

#include <cstdint>

enum class ControlCode : uint8_t
{
	MoveForward,
	MoveBack,
	MoveLeft,
	MoveRight,

	Action1,
	Action2,
	Action3,

	Pause,

	Count,
	Nothing = Count,
};

static constexpr uint8_t MaxControlCodes = 32;

static_assert((uint8_t)ControlCode::Count <= MaxControlCodes);