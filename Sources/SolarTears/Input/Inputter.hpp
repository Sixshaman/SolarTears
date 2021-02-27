#pragma once

#include <memory>
#include "ControlCodes.hpp"
#include "../../3rd party/DirectXMath/Inc/DirectXMath.h"

class Window;
class SceneDescription;
class LoggerQueue;
class KeyboardControl;
class MouseControl;
class InputScene;

class Inputter
{
public:
	Inputter(LoggerQueue* loggerQueue);
	~Inputter();

	void AttachToWindow(Window* window);

	bool GetKeyState(ControlCode key);

	void InitScene(SceneDescription* sceneDescription);
	void UpdateScene(float dt);

private:
	LoggerQueue* mLoggingBoard;

	std::unique_ptr<InputScene> mScene;

	std::unique_ptr<KeyboardControl> mKeyboardControl;
	std::unique_ptr<MouseControl>    mMouseControl;

	DirectX::XMFLOAT2 mAxis1Delta;
	DirectX::XMFLOAT2 mAxis2Delta;
	DirectX::XMFLOAT2 mAxis3Delta;
	uint32_t          mInputKeyState;
};