#pragma once

#define OEMRESOURCE

#include <string>
#include <Windows.h>
#include <wil/resource.h>

class Window
{
public:
	using ResizeStartedCallback  = void(*)(Window*, void*);
	using ResizeFinishedCallback = void(*)(Window*, void*);

public:
	Window(HINSTANCE hInstance, const std::wstring& title, int posX, int posY, int sizeX, int sizeY);
	~Window();

	HWND NativeHandle() const;

	HMONITOR NativeDisplay() const;

	uint32_t GetWidth()  const;
	uint32_t GetHeight() const;

	void RegisterResizeStartedCallback(ResizeStartedCallback callback,   void* callbackUserObject);
	void RegisterResizeFinishedCallback(ResizeFinishedCallback callback, void* callbackUserObject);

private:
	void CreateMainWindow(HINSTANCE hInstance, const std::wstring& title, int posX, int posY, int sizeX, int sizeY);

	LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

private:
	wil::unique_hwnd mMainWindowHandle;

	uint32_t mWindowWidth;
	uint32_t mWindowHeight;

	bool mResizingFlag;

	ResizeStartedCallback  mResizeStartedCallback;
	ResizeFinishedCallback mResizeFinishedCallback;

	void* mResizeStartedCallbackUserObject;
	void* mResizeFinishedCallbackUserObject;
};