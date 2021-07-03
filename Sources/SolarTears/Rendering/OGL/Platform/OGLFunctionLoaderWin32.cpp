#include "OGLFunctionLoaderWin32.hpp"
#include "OGLFunctionsWin32.hpp"
#include <cassert>
#include <wil/resource.h>

namespace wil
{
	using unique_hglrc = wil::unique_any<HGLRC, decltype(&::wglDeleteContext), ::wglDeleteContext>;
}

#define LOAD_OGL_FUNCTION(name) name = (decltype(name))(wglGetProcAddress(#name))

static void LoadWGLFunctionsFromContext()
{
#ifdef WGL_ARB_extensions_string

#ifdef WGL_ARB_create_context
	LOAD_OGL_FUNCTION(wglCreateContextAttribsARB);
#endif

#ifdef WGL_ARB_pixel_format
	LOAD_OGL_FUNCTION(wglGetPixelFormatAttribivARB);
	LOAD_OGL_FUNCTION(wglGetPixelFormatAttribfvARB);
	LOAD_OGL_FUNCTION(wglChoosePixelFormatARB);
#endif

#endif
}

#include "../OGLFunctionLoader.inl"

void OpenGL::LoadOpenGLFunctions()
{
	HINSTANCE appInstance = GetModuleHandle(nullptr);

	WNDCLASS tempWndClass;
	tempWndClass.style         = CS_OWNDC;
	tempWndClass.lpfnWndProc   = DefWindowProc;
	tempWndClass.cbClsExtra    = 0;
	tempWndClass.cbWndExtra    = 0;
	tempWndClass.hInstance     = appInstance;
	tempWndClass.hIcon         = nullptr;
	tempWndClass.hCursor       = nullptr;
	tempWndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	tempWndClass.lpszMenuName  = nullptr;
	tempWndClass.lpszClassName = L"TempOpenGLWindow";

	assert(RegisterClass(&tempWndClass) != 0);
	auto classCleanup = wil::scope_exit([appInstance]()
	{
		UnregisterClass(L"TempOpenGLWindow", appInstance);
	});
	
	wil::unique_hwnd tempWindow = wil::unique_hwnd(CreateWindow(L"TempOpenGLWindow", L"", WS_OVERLAPPED, 0, 0, 1, 1, nullptr, nullptr, appInstance, nullptr));
	assert(tempWindow != nullptr);

	wil::unique_hdc_window tempHdc = wil::unique_hdc_window(GetDC(tempWindow.get()));
	assert(tempHdc != nullptr);

	PIXELFORMATDESCRIPTOR tempPixelFormatDesc;
	tempPixelFormatDesc.nSize           = sizeof(PIXELFORMATDESCRIPTOR);
	tempPixelFormatDesc.nVersion        = 1;
	tempPixelFormatDesc.dwFlags         = PFD_DRAW_TO_WINDOW | LPD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_SWAP_LAYER_BUFFERS;
	tempPixelFormatDesc.iPixelType      = PFD_TYPE_RGBA;
	tempPixelFormatDesc.cColorBits      = 24;
	tempPixelFormatDesc.cRedBits        = 0;
	tempPixelFormatDesc.cGreenBits      = 0;
	tempPixelFormatDesc.cBlueBits       = 0;
	tempPixelFormatDesc.cAlphaBits      = 8;
	tempPixelFormatDesc.cAlphaShift     = 0;
	tempPixelFormatDesc.cAccumBits      = 0;
	tempPixelFormatDesc.cAccumRedBits   = 0;
	tempPixelFormatDesc.cAccumGreenBits = 0;
	tempPixelFormatDesc.cAccumBlueBits  = 0;
	tempPixelFormatDesc.cAccumAlphaBits = 0;
	tempPixelFormatDesc.cDepthBits      = 0;
	tempPixelFormatDesc.cStencilBits    = 0;
	tempPixelFormatDesc.cAuxBuffers     = 0;
	tempPixelFormatDesc.iLayerType      = PFD_MAIN_PLANE;
	tempPixelFormatDesc.bReserved       = 0;
	tempPixelFormatDesc.dwLayerMask     = 0;
	tempPixelFormatDesc.dwLayerMask     = 0;
	tempPixelFormatDesc.dwVisibleMask   = 0;
	tempPixelFormatDesc.dwDamageMask    = 0;

	int pixelFormatIndex = ChoosePixelFormat(tempHdc.get(), &tempPixelFormatDesc);
	assert(pixelFormatIndex != 0);

	if(!SetPixelFormat(tempHdc.get(), pixelFormatIndex, &tempPixelFormatDesc))
	{
		THROW_HR(HRESULT_FROM_WIN32(GetLastError()));
	}

	wil::unique_hglrc tempContext = wil::unique_hglrc(wglCreateContext(tempHdc.get()));
	assert(tempContext != nullptr);

	if (!wglMakeCurrent(tempHdc.get(), tempContext.get()))
	{
		THROW_HR(HRESULT_FROM_WIN32(GetLastError()));
	}

	LoadWGLFunctionsFromContext();
	LoadOGLFunctionsFromContext();
}