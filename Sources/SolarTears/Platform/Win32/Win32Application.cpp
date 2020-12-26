#include "Win32Application.hpp"
#include "../../Core/Engine.hpp"

Application::Application(HINSTANCE hInstance): mAppInstance(hInstance)
{
}

Application::~Application()
{
}

int Application::Run(Engine* engine)
{
	MSG msg = {};
	while(msg.message != WM_QUIT)
	{
		if(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			engine->Update();
		}
	}

	return (int)msg.wParam;
}

HINSTANCE Application::NativeHandle() const
{
	return mAppInstance;
}
