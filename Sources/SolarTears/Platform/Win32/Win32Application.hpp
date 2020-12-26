#pragma once

#include <Windows.h>

class Engine;

class Application 
{
public:
	Application(HINSTANCE hInstance);
	~Application();

	int Run(Engine* engine);

	HINSTANCE NativeHandle() const;

	Application(const Application& other)			 = delete;
	Application& operator=(const Application& other) = delete;

	Application(Application&& other)			= default;
	Application& operator=(Application&& other) = default;

private:
	HINSTANCE mAppInstance;
};