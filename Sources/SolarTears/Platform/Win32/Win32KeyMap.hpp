#pragma once

#include "../../Controls/ControlCodes.hpp"
#include "../../Controls/KeyboardKeyCodes.hpp"

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