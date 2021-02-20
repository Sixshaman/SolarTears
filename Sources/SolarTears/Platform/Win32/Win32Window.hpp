#pragma once

#define OEMRESOURCE

#include <string>
#include <Windows.h>
#include <wil/resource.h>
#include "../../Controls/KeyboardKeyCodes.hpp"
#include "../../Controls/ControlCodes.hpp"

class Window
{
public:
	using ResizeStartedCallback  = void(*)(Window*,              void*);
	using ResizeFinishedCallback = void(*)(Window*,              void*);
	using KeyPressedCallback     = void(*)(Window*, ControlCode, void*);
	using KeyReleasedCallback    = void(*)(Window*, ControlCode, void*);

public:
	Window(HINSTANCE hInstance, const std::wstring& title, int posX, int posY, int sizeX, int sizeY);
	~Window();

	HWND NativeHandle() const;

	HMONITOR NativeDisplay() const;

	uint32_t GetWidth()  const;
	uint32_t GetHeight() const;

	void RegisterResizeStartedCallback(ResizeStartedCallback callback);
	void RegisterResizeFinishedCallback(ResizeFinishedCallback callback);
	void RegisterKeyPressedCallback(KeyPressedCallback callback);
	void RegisterKeyReleasedCallback(KeyReleasedCallback callback);

	void SetCallbackUserPtr(void* userPtr);

	void MapKey(KeyCode keyCode, ControlCode controlCode);

private:
	void    InitializeKeyMap();
	uint8_t CalcExtendedKey(uint8_t keyCode, uint8_t scanCode, bool extendedKey);

	void CreateMainWindow(HINSTANCE hInstance, const std::wstring& title, int posX, int posY, int sizeX, int sizeY);

	LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

private:
	wil::unique_hwnd mMainWindowHandle;

	uint32_t mWindowWidth;
	uint32_t mWindowHeight;

	bool mResizingFlag;

	ResizeStartedCallback  mResizeStartedCallback;
	ResizeFinishedCallback mResizeFinishedCallback;
	KeyPressedCallback     mKeyPressedCallback;
	KeyReleasedCallback    mKeyReleasedCallback;

	void* mCallbackUserPtr;

	uint8_t     mAgnosticKeyMap[256];
	ControlCode mNativeKeyMap[256];
};