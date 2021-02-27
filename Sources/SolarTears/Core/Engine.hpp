#pragma once

#include <memory>
#include "Window.hpp"
#include "ThreadPool.hpp"
#include "Timer.hpp"

class Renderer;
class Inputter;
class Logger;
class LoggerQueue;
class Scene;
class FrameCounter;
class FPSCounter;

class Engine
{
public:
	Engine();
	~Engine();

	void BindToWindow(Window* window);
	void Update();

private:
	void CreateScene();

private:
	bool mPaused;

	std::unique_ptr<Scene>  mScene;

	std::unique_ptr<LoggerQueue> mLoggerQueue;
	std::unique_ptr<Logger>      mLogger;

	std::unique_ptr<Timer>      mTimer;
	std::unique_ptr<ThreadPool> mThreadPool;

	std::unique_ptr<Renderer> mRenderingSystem;
	std::unique_ptr<Inputter> mInputSystem;

	std::unique_ptr<FrameCounter> mFrameCounter;
	std::unique_ptr<FPSCounter>   mFPSCounter;
};