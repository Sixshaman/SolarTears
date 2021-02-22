#pragma once

#include "../../Input/ControlCodes.hpp"
#include "../../Input/KeyboardKeyCodes.hpp"

class KeyMap
{
public:
	KeyMap();
	~KeyMap();

	void MapKey(KeyCode keyCode, ControlCode controlCode);

	ControlCode GetControlCode(uint8_t nativeKeyCode) const;

private:
	void InitializeKeyMap();

private:
	uint8_t     mAgnosticKeyMap[256];
	ControlCode mNativeKeyMap[256];
};