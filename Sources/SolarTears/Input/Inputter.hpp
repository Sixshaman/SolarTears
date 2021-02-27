#pragma once

#include <memory>
#include "ControlCodes.hpp"

class Window;
class SceneDescription;
class LoggerQueue;
class KeyboardControl;
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

	uint32_t mInputKeyState;
};