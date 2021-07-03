#include "OGLFunctionsWin32.hpp"

#define DEFINE_WGL_FUNCTION(name) decltype(name) name = nullptr;

extern "C"
{
#ifdef WGL_ARB_extensions_string

#ifdef WGL_ARB_create_context
	DEFINE_WGL_FUNCTION(wglCreateContextAttribsARB)
#endif

#ifdef WGL_ARB_pixel_format
	DEFINE_WGL_FUNCTION(wglGetPixelFormatAttribivARB)
	DEFINE_WGL_FUNCTION(wglGetPixelFormatAttribfvARB)
	DEFINE_WGL_FUNCTION(wglChoosePixelFormatARB)
#endif

#endif
}