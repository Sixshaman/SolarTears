#pragma once

#include <memory>

class Window;
class KeyMap;
class SceneDescription;
class ControlScene;
class LoggerQueue;

class Input
{
public:
	Input(LoggerQueue* loggerQueue);
	~Input();

	void AttachToWindow(Window* window);

	void InitScene(SceneDescription* sceneDescription);
	void UpdateScene();

private:
	void InitKeyMap();

private:
	LoggerQueue* mLoggingBoard;

	std::unique_ptr<ControlScene> mScene;

	std::unique_ptr<KeyMap> mKeyMap;
};