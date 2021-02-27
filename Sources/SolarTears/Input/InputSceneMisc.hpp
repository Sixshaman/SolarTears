#pragma once

#include <cstdint>
#include "InputControlLocation.hpp"

struct InputSceneControlHandle
{
	uint32_t Id;

	bool operator==(const InputSceneControlHandle right) { return Id == right.Id; }
	bool operator!=(const InputSceneControlHandle right) { return Id != right.Id; }
};

using InputControlPressedCallback = void(*)(InputControlLocation*, float);
using InputAxisMoveCallback       = void(*)(InputControlLocation*, float, float, float);

static constexpr InputSceneControlHandle INVALID_SCENE_CONTROL_HANDLE = { .Id = (uint32_t)(-1) };