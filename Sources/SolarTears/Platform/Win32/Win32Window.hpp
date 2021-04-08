#pragma once

#define OEMRESOURCE

#include <string>
#include <Windows.h>
#include <wil/resource.h>
#include "../../Input/ControlCodes.hpp"

class KeyboardKeyMap;
class MouseKeyMap;

class Window
{
private:
	using KeyboardKeyMapCallback = ControlCode(*)(const KeyboardKeyMap*, uint8_t nativeCode);
	using MouseKeyMapCallback    = ControlCode(*)(const MouseKeyMap*,    uint8_t nativeCode);

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

	bool IsCursorVisible() const;

	void SetCursorVisible(bool cursorVisible);
	void CenterCursor();

	void SetMousePos(int32_t x, int32_t y);
	void GetMousePos(int32_t* outX, int32_t* outY);

	void RegisterResizeStartedCallback(ResizeStartedCallback callback);
	void RegisterResizeFinishedCallback(ResizeFinishedCallback callback);
	void RegisterKeyPressedCallback(KeyPressedCallback callback);
	void RegisterKeyReleasedCallback(KeyReleasedCallback callback);
	void RegisterMouseMoveCallback(MouseMoveCallback callback);

	void SetResizeCallbackUserPtr(void* userPtr);
	void SetKeyCallbackUserPtr(void* userPtr);
	void SetMouseCallbackUserPtr(void* userPtr);

	void SetKeyboardKeyMap(const KeyboardKeyMap* keyMap);
	void SetMouseKeyMap(const MouseKeyMap* keyMap);

private:
	uint8_t CalcExtendedKey(uint8_t keyCode, uint8_t scanCode, bool extendedKey);

	void CreateMainWindow(HINSTANCE hInstance, const std::wstring& title, int posX, int posY, int sizeX, int sizeY);

	LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

private:
	wil::unique_hwnd mMainWindowHandle;

	uint32_t mWindowWidth;
	uint32_t mWindowHeight;
	int32_t  mWindowPosX;
	int32_t  mWindowPosY;

	bool mResizingFlag;
	bool mSetCursorPosFlag;
	bool mCursorVisible;

	ResizeStartedCallback  mResizeStartedCallback;
	ResizeFinishedCallback mResizeFinishedCallback;
	KeyPressedCallback     mKeyPressedCallback;
	KeyReleasedCallback    mKeyReleasedCallback;
	MouseMoveCallback      mMouseMoveCallback;

	void* mResizeCallbackUserPtr;
	void* mKeyCallbackUserPtr;
	void* mMouseCallbackUserPtr;

	KeyboardKeyMapCallback mKeyboardKeyMapCallback;
	MouseKeyMapCallback    mMouseKeyMapCallback;

	const KeyboardKeyMap* mKeyboardKeyMapRef;
	const MouseKeyMap*    mMouseKeyMapRef;
};