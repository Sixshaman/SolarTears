#include "Win32Window.hpp"
#include <wil/result.h>
#include <windowsx.h>

namespace
{
	void DefaultResizeStartedCallback(Window*, void*)
	{
	}

	void DefaultResizeFinishedCallback(Window*, void*)
	{
	}

	void DefaultKeyPressedCallback(Window*, ControlCode, void*)
	{
	}

	void DefaultKeyReleasedCallback(Window*, ControlCode, void*)
	{
	}

	ControlCode DefaultKeyMapCallback(const KeyMap*, uint8_t)
	{
		return ControlCode::Nothing;
	}
}

Window::Window(HINSTANCE hInstance, const std::wstring& title, int posX, int posY, int sizeX, int sizeY): mResizeStartedCallback(DefaultResizeStartedCallback), mResizeFinishedCallback(DefaultResizeFinishedCallback),
                                                                                                          mKeyPressedCallback(DefaultKeyPressedCallback), mKeyReleasedCallback(DefaultKeyReleasedCallback),
	                                                                                                      mCallbackUserPtr(nullptr), mResizingFlag(false), mKeyMapCallback(DefaultKeyMapCallback), mKeyMapRef(nullptr)
{
	CreateMainWindow(hInstance, title, posX, posY, sizeX, sizeY);
}

Window::~Window()
{
}

HWND Window::NativeHandle() const
{
	return mMainWindowHandle.get();
}

HMONITOR Window::NativeDisplay() const
{
	return MonitorFromWindow(mMainWindowHandle.get(), MONITOR_DEFAULTTONEAREST);
}

uint32_t Window::GetWidth() const
{
	return mWindowWidth;
}

uint32_t Window::GetHeight() const
{
	return mWindowHeight;
}

void Window::RegisterResizeStartedCallback(ResizeStartedCallback callback)
{
	mResizeStartedCallback = callback;
}

void Window::RegisterResizeFinishedCallback(ResizeFinishedCallback callback)
{
	mResizeFinishedCallback = callback;
}

void Window::RegisterKeyPressedCallback(KeyPressedCallback callback)
{
	mKeyPressedCallback = callback;
}

void Window::RegisterKeyReleasedCallback(KeyReleasedCallback callback)
{
	mKeyReleasedCallback = callback;
}

void Window::SetCallbackUserPtr(void* userPtr)
{
	mCallbackUserPtr = userPtr;
}

void Window::SetKeyMap(const KeyMap* keyMap)
{
	if(keyMap)
	{
		mKeyMapCallback = [](const KeyMap* keyMap, uint8_t nativeCode)
		{
			return keyMap->GetControlCode(nativeCode);
		};
	}
	else
	{
		mKeyMapCallback = DefaultKeyMapCallback;
	}

	mKeyMapRef = keyMap;
}

uint8_t Window::CalcExtendedKey(uint8_t keyCode, uint8_t scanCode, bool extendedKey)
{
	switch(keyCode)
	{
	case VK_SHIFT:
		return (uint8_t)MapVirtualKey(scanCode, MAPVK_VSC_TO_VK_EX);
	case VK_CONTROL:
		return extendedKey ? VK_RCONTROL : VK_LCONTROL;
	case VK_MENU:
		return extendedKey ? VK_RMENU : VK_LMENU;
	case VK_RETURN:
		return extendedKey ? VK_SEPARATOR : VK_RETURN;
	default:
		return keyCode;
	}
}

void Window::CreateMainWindow(HINSTANCE hInstance, const std::wstring& title, int posX, int posY, int sizeX, int sizeY)
{
	LPCWSTR windowClassName = L"SolarTearsEngine";

	mWindowWidth  = sizeX;
	mWindowHeight = sizeY;
	
	WNDCLASSEX wc;
	wc.cbSize        = sizeof(WNDCLASSEX);
	wc.style         = CS_HREDRAW | CS_VREDRAW | CS_PARENTDC;                                                                               
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = nullptr;
	wc.hIconSm       = nullptr;
	wc.hCursor       = (HCURSOR)LoadImage(nullptr, MAKEINTRESOURCE(OCR_NORMAL), IMAGE_CURSOR, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
	wc.hbrBackground = (HBRUSH)(COLOR_DESKTOP + 1);
	wc.lpszMenuName  = nullptr;
	wc.lpszClassName = windowClassName;
	wc.lpfnWndProc   = [](HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) -> LRESULT
	{
		if(message == WM_CREATE) [[unlikely]]
		{
			LPCREATESTRUCT createStruct = (LPCREATESTRUCT)(lparam);
			Window* that = (Window*)(createStruct->lpCreateParams);

			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)(that));
			return 0;
		}
		else [[likely]]
		{
			Window* that = (Window*)(GetWindowLongPtr(hwnd, GWLP_USERDATA));
			return that->WindowProc(hwnd, message, wparam, lparam);
		}
	};

	if(!RegisterClassEx(&wc))
	{
		THROW_IF_FAILED((HRESULT)GetLastError());
	}

	DWORD wndStyleEx = 0;
	DWORD wndStyle   = WS_OVERLAPPEDWINDOW;

	RECT windowRect;
	windowRect.left   = posX;
	windowRect.right  = windowRect.left + sizeX;
	windowRect.top    = posY;
	windowRect.bottom = windowRect.top + sizeY;
	AdjustWindowRectEx(&windowRect, wndStyle, FALSE, wndStyleEx);

	int windowWidth  = windowRect.right  - windowRect.left;
	int windowHeight = windowRect.bottom - windowRect.top;

	LPVOID userParam  = this;
	mMainWindowHandle.reset(CreateWindowEx(wndStyleEx, windowClassName, title.c_str(), wndStyle, windowRect.left, windowRect.top, windowWidth, windowHeight, nullptr, nullptr, hInstance, userParam));

	UpdateWindow(mMainWindowHandle.get());
	ShowWindow(mMainWindowHandle.get(), SW_SHOW);
}

LRESULT Window::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch(message)
	{
	case WM_CLOSE:
	{
		PostQuitMessage(0);
		return 0;
	}
	case WM_ENTERSIZEMOVE:
	{
		mResizingFlag = true;

		mResizeStartedCallback(this, mCallbackUserPtr);
		break;
	}
	case WM_EXITSIZEMOVE:
	{
		mResizingFlag = false;

		mResizeFinishedCallback(this, mCallbackUserPtr);
		break;
	}
	case WM_SIZE:
	{
		mWindowWidth  = GET_X_LPARAM(lparam);
		mWindowHeight = GET_Y_LPARAM(lparam);

		switch (wparam)
		{
		case SIZE_MAXHIDE:
			OutputDebugString(L"WINDOW: Maxhide\n");
			break;

		case SIZE_MAXIMIZED:
		{
			mResizeFinishedCallback(this, mCallbackUserPtr);
			break;
		}

		case SIZE_MAXSHOW:
			OutputDebugString(L"WINDOW: Maxshow\n");
			break;

		case SIZE_MINIMIZED:
			OutputDebugString(L"WINDOW: Minimized\n");
			break;

		case SIZE_RESTORED:
		{
			if(!mResizingFlag)
			{
				mResizeFinishedCallback(this, mCallbackUserPtr);
			}

			break;
		}

		default:
			break;
		}

		return 0;
	}
	case WM_SYSKEYDOWN:
	case WM_KEYDOWN:
	{
		uint8_t scancode    = (lparam >> 16) & 0xff;
		bool    extendedKey = lparam & 0x01000000;

		uint8_t keycode = CalcExtendedKey((uint8_t)wparam, scancode, extendedKey);

		ControlCode controlCode = mKeyMapCallback(mKeyMapRef, keycode);
		mKeyPressedCallback(this, controlCode, mCallbackUserPtr);
		return 0;
	}
	case WM_SYSKEYUP:
	case WM_KEYUP:
	{
		uint8_t scancode    = (lparam >> 16) & 0xff;
		bool    extendedKey = lparam & 0x01000000;

		uint8_t keycode = CalcExtendedKey((uint8_t)wparam, scancode, extendedKey);

		ControlCode controlCode = mKeyMapCallback(mKeyMapRef, keycode);
		mKeyReleasedCallback(this, controlCode, mCallbackUserPtr);
		return 0;
	}
	default:
	{
		break;
	}
	}

	return DefWindowProc(hwnd, message, wparam, lparam);
}
