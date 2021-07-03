#include "OGLContextWin32.hpp"
#include "OGLFunctionsWin32.hpp"
#include <cassert>
#include <array>

OpenGL::GLContext::GLContext(HWND hwnd)
{
	mHdc = wil::unique_hdc_window(GetDC(hwnd));

	std::array pixelFormatCountIds = {WGL_NUMBER_PIXEL_FORMATS_ARB};
	std::array pixelFormatCounts   = {0};
	if(!wglGetPixelFormatAttribivARB(mHdc.get(), 0, 0, (UINT)pixelFormatCountIds.size(), pixelFormatCountIds.data(), pixelFormatCounts.data()))
	{
		THROW_HR(HRESULT_FROM_WIN32(GetLastError()));
	}

	//TODO: stereo here?
	//TODO: HDR here?
	std::array attributes = {WGL_DRAW_TO_WINDOW_ARB, TRUE,
	                         WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_EXT,
		                     WGL_SWAP_METHOD_ARB,    WGL_SWAP_EXCHANGE_ARB,
		                     WGL_SUPPORT_OPENGL_ARB, TRUE,
		                     WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
							 WGL_COLOR_BITS_ARB,     24,
							 WGL_RED_BITS_ARB,       8,
		                     WGL_GREEN_BITS_ARB,     8,
		                     WGL_BLUE_BITS_ARB,      8,
	                         0};

	std::array formatIndices      = {0};
	UINT       numReturnedFormats = 0;
	if(!wglChoosePixelFormatARB(mHdc.get(), attributes.data(), nullptr, (UINT)formatIndices.size(), formatIndices.data(), &numReturnedFormats))
	{
		THROW_HR(HRESULT_FROM_WIN32(GetLastError()));
	}

	PIXELFORMATDESCRIPTOR pfd;
	DescribePixelFormat(mHdc.get(), formatIndices[0], sizeof(PIXELFORMATDESCRIPTOR), &pfd);
	if(!SetPixelFormat(mHdc.get(), formatIndices[0], &pfd))
	{
		THROW_HR(HRESULT_FROM_WIN32(GetLastError()));
	}
	
#if defined(DEBUG) || defined(_DEBUG)
	constexpr int contextFlags = WGL_CONTEXT_DEBUG_BIT_ARB;
#else
	constexpr int contextFlags = 0;
#endif

	std::array contextAttribs = {WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
								 WGL_CONTEXT_MINOR_VERSION_ARB, 6,
								 WGL_CONTEXT_LAYER_PLANE_ARB,   0,
								 WGL_CONTEXT_FLAGS_ARB,         contextFlags,
								 WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
	                             0};

	mHglrc = wil::unique_hglrc(wglCreateContextAttribsARB(mHdc.get(), nullptr, contextAttribs.data()));
	if(mHglrc == nullptr)
	{
		THROW_HR(HRESULT_FROM_WIN32(GetLastError()));
	}
}

OpenGL::GLContext::~GLContext()
{
}

void OpenGL::GLContext::MakeCurrent()
{
	[[unlikely]]
	if(!wglMakeCurrent(mHdc.get(), mHglrc.get()))
	{
		THROW_HR(HRESULT_FROM_WIN32(GetLastError()));
	}
}

void OpenGL::GLContext::DoneCurrent()
{
	wglMakeCurrent(mHdc.get(), nullptr);
}

void OpenGL::GLContext::SwapBuffers()
{
	[[unlikely]]
	if(!wglSwapLayerBuffers(mHdc.get(), 0))
	{
		THROW_HR(HRESULT_FROM_WIN32(GetLastError()));
	}
}
