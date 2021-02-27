#include "MouseControl.hpp"
#include "MouseKeyMap.hpp"

MouseControl::MouseControl(): mControlState(0)
{
}

MouseControl::~MouseControl()
{
}

void MouseControl::InitKeyMap()
{
	mKeyMap = std::make_unique<MouseKeyMap>();

	mKeyMap->MapLButton(ControlCode::Action1);
	mKeyMap->MapRButton(ControlCode::Action2);
	mKeyMap->MapMButton(ControlCode::Action3);
}