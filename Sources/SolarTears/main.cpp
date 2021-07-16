#include "Core/Application.hpp"
#include "Core/Window.hpp"
#include "Core/Engine.hpp"
#include <memory>

#ifdef _WIN32
int WinMain(HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance, [[maybe_unused]] LPSTR nCmdLine, [[maybe_unused]] int nCmdShow)
{
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