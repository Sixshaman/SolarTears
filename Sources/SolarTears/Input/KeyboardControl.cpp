#include "KeyboardControl.hpp"
#include "ControlCodes.hpp"
#include "KeyboardKeyCodes.hpp"
#include "KeyboardKeyMap.hpp"
#include "../Core/Window.hpp"

KeyboardControl::KeyboardControl()
{
	mControlState = 0;

	InitKeyMap();
}

KeyboardControl::~KeyboardControl()
{
}

void KeyboardControl::AttachToWindow(Window* window)
{
	window->SetKeyCallbackUserPtr(this);

	window->RegisterKeyPressedCallback([](Window* window, ControlCode controlCode, void* userObject)
	{
		UNREFERENCED_PARAMETER(window);

		KeyboardControl* that = reinterpret_cast<KeyboardControl*>(userObject);
		that->mControlState |= (1u << (uint32_t)(controlCode));
	});

	window->RegisterKeyReleasedCallback([](Window* window, ControlCode controlCode, void* userObject)
	{
		UNREFERENCED_PARAMETER(window);

		KeyboardControl* that = reinterpret_cast<KeyboardControl*>(userObject);
		that->mControlState &= ~(1u << (uint32_t)(controlCode));
	});

	window->SetKeyMap(mKeyMap.get());
}

uint32_t KeyboardControl::GetControlState()
{
	return mControlState;
}

void KeyboardControl::InitKeyMap()
{
	mKeyMap = std::make_unique<KeyboardKeyMap>();

	mKeyMap->MapKey(KeyCode::W, ControlCode::MoveForward);
	mKeyMap->MapKey(KeyCode::A, ControlCode::MoveLeft);
	mKeyMap->MapKey(KeyCode::S, ControlCode::MoveBack);
	mKeyMap->MapKey(KeyCode::D, ControlCode::MoveRight);
}
