#include "Input.hpp"
#include "KeyMap.hpp"
#include "KeyboardKeyCodes.hpp"
#include "ControlCodes.hpp"
#include "ControlScene.hpp"
#include "../Core/Window.hpp"
#include "../Core/Util.hpp"

Input::Input(LoggerQueue* loggerQueue): mLoggingBoard(loggerQueue)
{
	InitKeyMap();
}

Input::~Input()
{
}

void Input::AttachToWindow(Window* window)
{
	window->SetKeyMap(mKeyMap.get());

	window->SetKeyCallbackUserPtr(this);

	window->RegisterKeyPressedCallback([](Window* window, ControlCode controlCode, void* userObject)
	{
		UNREFERENCED_PARAMETER(window);
		UNREFERENCED_PARAMETER(controlCode);
		UNREFERENCED_PARAMETER(userObject);
	});

	window->RegisterKeyReleasedCallback([](Window* window, ControlCode controlCode, void* userObject)
	{
		UNREFERENCED_PARAMETER(window);
		UNREFERENCED_PARAMETER(controlCode);
		UNREFERENCED_PARAMETER(userObject);
	});
}

void Input::InitScene(SceneDescription* sceneDescription)
{
	UNREFERENCED_PARAMETER(sceneDescription);
}

void Input::UpdateScene()
{
}

void Input::InitKeyMap()
{
	mKeyMap = std::make_unique<KeyMap>();

	mKeyMap->MapKey(KeyCode::W, ControlCode::MoveForward);
	mKeyMap->MapKey(KeyCode::A, ControlCode::MoveLeft);
	mKeyMap->MapKey(KeyCode::S, ControlCode::MoveBack);
	mKeyMap->MapKey(KeyCode::D, ControlCode::MoveRight);
}
