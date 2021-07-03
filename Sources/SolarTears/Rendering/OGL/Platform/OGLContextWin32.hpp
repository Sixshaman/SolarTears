#pragma once

#include "../../../Core/Window.hpp"
#include "wil/resource.h"

namespace wil
{
	using unique_hglrc = wil::unique_any<HGLRC, decltype(&::wglDeleteContext), ::wglDeleteContext>;
}

namespace OpenGL
{
	class GLContext
	{
	public:
		GLContext(HWND windowHandle);
		~GLContext();

		void MakeCurrent();
		void DoneCurrent();

		void SwapBuffers();

	private:
		wil::unique_hdc_window mHdc;

		wil::unique_hglrc mHglrc;
	};
}