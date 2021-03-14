#include "MouseControl.hpp"
#include "MouseKeyMap.hpp"
#include "../Core/Window.hpp"

MouseControl::MouseControl(): mControlState(0), mXDelta(0.0f), mYDelta(0.0f), mWheelDelta(0.0f)
{
}

MouseControl::~MouseControl()
{
}

void MouseControl::AttachToWindow(Window* window)
{
	window->SetMouseCallbackUserPtr(this);

	window->RegisterMouseMoveCallback([](Window* window, int mousePosX, int mousePosY, void* userPtr)
	{
		MouseControl* that = reinterpret_cast<MouseControl*>(userPtr);
		that->mXDelta = (float)(mousePosX - that->mPrevMousePosX);
		that->mYDelta = (float)(mousePosY - that->mPrevMousePosY);

		window->CenterCursor();
		window->GetMousePos(&that->mPrevMousePosX, &that->mPrevMousePosY);
	});

	window->SetMouseKeyMap(mKeyMap.get());

	window->SetCursorVisible(false);
	window->CenterCursor();

	window->GetMousePos(&mPrevMousePosX, &mPrevMousePosY);
}

uint32_t MouseControl::GetControlState() const
{
	return mControlState;
}

float MouseControl::GetXDelta() const
{
	return mXDelta;
}

float MouseControl::GetYDelta() const
{
	return mYDelta;
}

float MouseControl::GetWheelDelta() const
{
	return mWheelDelta;
}

void MouseControl::Reset()
{
	mXDelta = 0.0f;
	mYDelta = 0.0f;
}

void MouseControl::InitKeyMap()
{
	mKeyMap = std::make_unique<MouseKeyMap>();

	mKeyMap->MapLButton(ControlCode::Action1);
	mKeyMap->MapRButton(ControlCode::Action2);
	mKeyMap->MapMButton(ControlCode::Action3);
}