#pragma once

#include "../../Input/ControlCodes.hpp"

class MouseKeyMap
{
public:
	MouseKeyMap();
	~MouseKeyMap();

	void MapLButton(ControlCode controlCode);
	void MapRButton(ControlCode controlCode);
	void MapMButton(ControlCode controlCode);

	ControlCode GetControlCode(uint8_t nativeMouseCode) const;

private:
	void InitializeKeyMap();

private:
	ControlCode mLButtonAction;
	ControlCode mRButtonAction;
	ControlCode mMButtonAction;
};