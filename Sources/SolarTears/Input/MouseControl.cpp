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
		int windowWidth  = window->GetWidth();
		int windowHeight = window->GetHeight();

		window->SetMousePos(windowWidth / 2, windowHeight / 2);

		mousePosX; mousePosY;

		MouseControl* that = reinterpret_cast<MouseControl*>(userPtr);
		that->mXDelta = 0.0f; // (float)(mousePosX - windowWidth / 2);
		that->mYDelta = 0.0f; // (float)(mousePosY - windowHeight / 2);
	});

	window->SetMouseKeyMap(mKeyMap.get());

	window->SetCursorVisible(false);
	window->SetMousePos(window->GetWidth() / 2, window->GetHeight() / 2);
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

void MouseControl::InitKeyMap()
{
	mKeyMap = std::make_unique<MouseKeyMap>();

	mKeyMap->MapLButton(ControlCode::Action1);
	mKeyMap->MapRButton(ControlCode::Action2);
	mKeyMap->MapMButton(ControlCode::Action3);
}