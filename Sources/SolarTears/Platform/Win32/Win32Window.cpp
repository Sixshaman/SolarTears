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
}

Window::Window(HINSTANCE hInstance, const std::wstring& title, int posX, int posY, int sizeX, int sizeY): mResizeStartedCallback(DefaultResizeStartedCallback), mResizeFinishedCallback(DefaultResizeFinishedCallback),
                                                                                                          mResizeStartedCallbackUserObject(nullptr), mResizeFinishedCallbackUserObject(nullptr),
	                                                                                                      mResizingFlag(false)
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

void Window::RegisterResizeStartedCallback(ResizeStartedCallback callback, void* callbackUserObject)
{
	mResizeStartedCallback           = callback;
	mResizeStartedCallbackUserObject = callbackUserObject;
}

void Window::RegisterResizeFinishedCallback(ResizeFinishedCallback callback, void* callbackUserObject)
{
	mResizeFinishedCallback           = callback;
	mResizeFinishedCallbackUserObject = callbackUserObject;
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

		mResizeStartedCallback(this, mResizeStartedCallbackUserObject);
		break;
	}
	case WM_EXITSIZEMOVE:
	{
		mResizingFlag = false;

		mResizeFinishedCallback(this, mResizeFinishedCallbackUserObject);
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
			mResizeFinishedCallback(this, mResizeFinishedCallbackUserObject);
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
				mResizeFinishedCallback(this, mResizeFinishedCallbackUserObject);
			}

			break;
		}

		default:
			break;
		}

		return 0;
	}
	default:
	{
		break;
	}
	}

	return DefWindowProc(hwnd, message, wparam, lparam);
}
