#pragma once

#include <cstdint>

enum class ControlCode: uint8_t
{
	Nothing,

	MoveForward,
	MoveBack,
	MoveLeft,
	MoveRight
};