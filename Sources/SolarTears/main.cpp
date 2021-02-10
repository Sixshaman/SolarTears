#include "Core/Application.hpp"
#include "Core/Window.hpp"
#include "Core/Engine.hpp"
#include <memory>

#ifdef _WIN32
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR nCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(nCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	Application app(hInstance);
#else
int main(int argc, char* argv[])
{
#endif

	std::unique_ptr<Engine> engine = std::make_unique<Engine>();

	Window window(app.NativeHandle(), L"Solar Tears", 100, 100, 768, 768);
	engine->BindToWindow(&window);

	return app.Run(engine.get());
}