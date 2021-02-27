#include "Win32MouseKeyMap.hpp"
#include <Windows.h>

MouseKeyMap::MouseKeyMap()
{
	InitializeKeyMap();
}

MouseKeyMap::~MouseKeyMap()
{
}

void MouseKeyMap::MapLButton(ControlCode controlCode)
{
	mLButtonAction = controlCode;
}

void MouseKeyMap::MapRButton(ControlCode controlCode)
{
	mRButtonAction = controlCode;
}

void MouseKeyMap::MapMButton(ControlCode controlCode)
{
	mMButtonAction = controlCode;
}

ControlCode MouseKeyMap::GetControlCode(uint8_t nativeMouseCode) const
{
	switch(nativeMouseCode)
	{
	case VK_LBUTTON:
		return mLButtonAction;
	case VK_RBUTTON:
		return mRButtonAction;
	case VK_MBUTTON:
		return mMButtonAction;
	default:
		return ControlCode::Nothing;
	}
}

void MouseKeyMap::InitializeKeyMap()
{
	mLButtonAction = ControlCode::Nothing;
	mRButtonAction = ControlCode::Nothing;
	mMButtonAction = ControlCode::Nothing;
}
