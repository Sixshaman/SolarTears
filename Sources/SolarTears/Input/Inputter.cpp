#include "Inputter.hpp"
#include "KeyboardKeyMap.hpp"
#include "KeyboardKeyCodes.hpp"
#include "ControlCodes.hpp"
#include "InputScene.hpp"
#include "KeyboardControl.hpp"
#include "../Core/Window.hpp"
#include "../Core/Util.hpp"
#include "../Core/Scene/SceneDescription.hpp"
#include "InputSceneBuilder.hpp"

Inputter::Inputter(LoggerQueue* loggerQueue): mLoggingBoard(loggerQueue)
{
	mKeyboardControl = std::make_unique<KeyboardControl>();

	mInputKeyState = 0u;
}

Inputter::~Inputter()
{
}

void Inputter::AttachToWindow(Window* window)
{
	mKeyboardControl->AttachToWindow(window);
}

bool Inputter::GetKeyState(ControlCode key)
{
	return mInputKeyState & (1u << (uint32_t)key);
}

void Inputter::InitScene(SceneDescription* sceneDescription)
{
	mScene = std::make_unique<InputScene>();
	sceneDescription->BindInputComponent(mScene.get());

	InputSceneBuilder sceneBuilder(mScene.get());
	sceneDescription->BuildInputComponent(&sceneBuilder);
}

void Inputter::UpdateScene(float dt)
{
	mInputKeyState = mKeyboardControl->GetControlState();

	mScene->UpdateScene(mInputKeyState, dt);
}