#pragma once

#include <Windows.h>
#include <GL/GL.h>
#include <GL/wgl.h>

extern "C"
{
#ifdef WGL_ARB_extensions_string

#ifdef WGL_ARB_create_context
	extern PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
#endif

#ifdef WGL_ARB_pixel_format
	extern PFNWGLGETPIXELFORMATATTRIBIVARBPROC wglGetPixelFormatAttribivARB;
	extern PFNWGLGETPIXELFORMATATTRIBFVARBPROC wglGetPixelFormatAttribfvARB;
	extern PFNWGLCHOOSEPIXELFORMATARBPROC      wglChoosePixelFormatARB;
#endif

#endif
}