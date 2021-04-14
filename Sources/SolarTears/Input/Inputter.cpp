#include "Inputter.hpp"
#include "KeyboardKeyMap.hpp"
#include "KeyboardKeyCodes.hpp"
#include "ControlCodes.hpp"
#include "KeyboardControl.hpp"
#include "MouseControl.hpp"
#include "../Core/Window.hpp"
#include "../Core/Util.hpp"
#include "../Core/Scene/Scene.hpp"

Inputter::Inputter(LoggerQueue* loggerQueue): mLoggingBoard(loggerQueue)
{
	mKeyboardControl = std::make_unique<KeyboardControl>();
	mMouseControl    = std::make_unique<MouseControl>();

	mCurrentControlState.Axis1Delta          = DirectX::XMFLOAT2(0.0f, 0.0f);
	mCurrentControlState.Axis2Delta          = DirectX::XMFLOAT2(0.0f, 0.0f);
	mCurrentControlState.Axis3Delta          = DirectX::XMFLOAT2(0.0f, 0.0f);
	mCurrentControlState.CurrentKeyStates    = 0x00000000;
	mCurrentControlState.KeyStateChangeFlags = 0x00000000;
}

Inputter::~Inputter()
{
}

void Inputter::AttachToWindow(Window* window)
{
	mKeyboardControl->AttachToWindow(window);
	mMouseControl->AttachToWindow(window);
}

bool Inputter::GetKeyState(ControlCode key) const
{
	return mCurrentControlState.CurrentKeyStates & (1u << (uint32_t)key);
}

bool Inputter::GetKeyStateChange(ControlCode key) const
{
	return mCurrentControlState.KeyStateChangeFlags & (1u << (uint32_t)key);
}

DirectX::XMFLOAT2 Inputter::GetAxis2Delta() const
{
	return mCurrentControlState.Axis2Delta;
}

void Inputter::SetPaused(bool paused)
{
	mMouseControl->SetPaused(paused);
}

void Inputter::UpdateControls()
{
	uint32_t PreviousInputKeyStates = mCurrentControlState.CurrentKeyStates;
	 
	mCurrentControlState.CurrentKeyStates    = mKeyboardControl->GetControlState();
	mCurrentControlState.KeyStateChangeFlags = mCurrentControlState.CurrentKeyStates & (PreviousInputKeyStates ^ mCurrentControlState.CurrentKeyStates);

	mCurrentControlState.Axis1Delta = DirectX::XMFLOAT2(0.0f, 0.0f);
	mCurrentControlState.Axis2Delta = DirectX::XMFLOAT2(2.0f * mMouseControl->GetXDelta(), -4.0f * mMouseControl->GetYDelta());
	mCurrentControlState.Axis3Delta = DirectX::XMFLOAT2(0.0f, mMouseControl->GetWheelDelta());

	mMouseControl->Reset();
}