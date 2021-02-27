#pragma once

#define OEMRESOURCE

#include <string>
#include <Windows.h>
#include <wil/resource.h>
#include "../../Input/ControlCodes.hpp"
#include "../../Input/KeyboardKeyMap.hpp"

class Window
{
private:
	using KeyMapCallback = ControlCode(*)(const KeyboardKeyMap*, uint8_t nativeCode);

public:
	using ResizeStartedCallback  = void(*)(Window*,              void*);
	using ResizeFinishedCallback = void(*)(Window*,              void*);
	using KeyPressedCallback     = void(*)(Window*, ControlCode, void*);
	using KeyReleasedCallback    = void(*)(Window*, ControlCode, void*);
	using MouseMoveCallback      = void(*)(Window*, int, int,    void*);

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
	void RegisterMouseMoveCallback(MouseMoveCallback callback);

	void SetResizeCallbackUserPtr(void* userPtr);
	void SetKeyCallbackUserPtr(void* userPtr);
	void SetMouseCallbackUserPtr(void* userPtr);

	void SetKeyMap(const KeyboardKeyMap* keyMap);

private:
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
	MouseMoveCallback      mMouseMoveCallback;

	void* mResizeCallbackUserPtr;
	void* mKeyCallbackUserPtr;
	void* mMouseCallbackUserPtr;

	KeyMapCallback mKeyMapCallback;

	const KeyboardKeyMap* mKeyMapRef;
};