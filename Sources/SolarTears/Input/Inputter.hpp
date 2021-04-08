#pragma once

#include <memory>
#include "ControlCodes.hpp"
#include "ControlState.hpp"

class Window;
class LoggerQueue;
class KeyboardControl;
class MouseControl;
class Engine;

class Inputter
{
public:
	Inputter(LoggerQueue* loggerQueue);
	~Inputter();

	void AttachToWindow(Window* window);

	bool GetKeyState(ControlCode key)       const;
	bool GetKeyStateChange(ControlCode key) const;

	DirectX::XMFLOAT2 GetAxis2Delta() const;

	void SetPaused(bool paused);

	void UpdateControls();

private:
	LoggerQueue* mLoggingBoard;

	std::unique_ptr<KeyboardControl> mKeyboardControl;
	std::unique_ptr<MouseControl>    mMouseControl;

	ControlState mCurrentControlState;
};