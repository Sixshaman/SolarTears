#include "OGLRenderer.hpp"
#include "OGLContext.hpp"
#include "OGLFunctionLoader.hpp"
#include "OGLFunctions.hpp"

OpenGL::Renderer::Renderer(LoggerQueue* loggerQueue, FrameCounter* frameCounter): ::Renderer(loggerQueue), mFrameCounterRef(frameCounter)
{
	LoadOpenGLFunctions();
}

OpenGL::Renderer::~Renderer()
{
}

void OpenGL::Renderer::AttachToWindow(Window* window)
{
	mGLContext = std::make_unique<GLContext>(window->NativeHandle());
}

void OpenGL::Renderer::ResizeWindowBuffers(Window* window)
{
	UNREFERENCED_PARAMETER(window);
}

void OpenGL::Renderer::InitScene(SceneDescription* sceneDescription)
{
	UNREFERENCED_PARAMETER(sceneDescription);
}

void OpenGL::Renderer::RenderScene()
{
}

void OpenGL::Renderer::InitFrameGraph(FrameGraphConfig&& frameGraphConfig, FrameGraphDescription&& frameGraphDescription)
{
	UNREFERENCED_PARAMETER(frameGraphConfig);
	UNREFERENCED_PARAMETER(frameGraphDescription);
}