#include "Inputter.hpp"
#include "KeyboardKeyMap.hpp"
#include "KeyboardKeyCodes.hpp"
#include "ControlCodes.hpp"
#include "InputScene.hpp"
#include "KeyboardControl.hpp"
#include "MouseControl.hpp"
#include "../Core/Window.hpp"
#include "../Core/Util.hpp"
#include "../Core/Scene/SceneDescription.hpp"
#include "InputSceneBuilder.hpp"

Inputter::Inputter(LoggerQueue* loggerQueue): mLoggingBoard(loggerQueue)
{
	mKeyboardControl = std::make_unique<KeyboardControl>();
	mMouseControl    = std::make_unique<MouseControl>();

	mInputKeyState = 0u;
}

Inputter::~Inputter()
{
}

void Inputter::AttachToWindow(Window* window)
{
	mKeyboardControl->AttachToWindow(window);
	mMouseControl->AttachToWindow(window);
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

	mAxis1Delta = DirectX::XMFLOAT2(0.0f,                               0.0f);
	mAxis2Delta = DirectX::XMFLOAT2(mMouseControl->GetXDelta() * 5.0f, -mMouseControl->GetYDelta() * 5.0f);
	mAxis3Delta = DirectX::XMFLOAT2(0.0f,                               mMouseControl->GetWheelDelta());

	mScene->UpdateScene(mInputKeyState, mAxis2Delta.x, mAxis2Delta.y, dt);

	mMouseControl->Reset();
}