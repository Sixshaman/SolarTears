#include "OGLRenderer.hpp"
#include "OGLContext.hpp"
#include "OGLFunctionLoader.hpp"
#include "OGLFunctions.hpp"
#include "OGLUtils.hpp"
#include "Scene/OGLScene.hpp"
#include "Scene/OGLSceneBuilder.hpp"

OpenGL::Renderer::Renderer(LoggerQueue* loggerQueue, FrameCounter* frameCounter): ::Renderer(loggerQueue), mFrameCounterRef(frameCounter)
{
	LoadOpenGLFunctions();

#if defined(DEBUG) || defined(_DEBUG)
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
	glDebugMessageCallback(OGLUtils::GLDebugCallback, nullptr);
#endif
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
	mGLContext->MakeCurrent();

	mScene = std::make_unique<RenderableScene>();
	sceneDescription->BindRenderableComponent(mScene.get());

	RenderableSceneBuilder sceneBuilder(mScene.get());
	sceneDescription->BuildRenderableComponent(&sceneBuilder);

	mGLContext->DoneCurrent();
}

void OpenGL::Renderer::RenderScene()
{
	mGLContext->MakeCurrent();

	mGLContext->SwapBuffers();
	mGLContext->DoneCurrent();
}

void OpenGL::Renderer::InitFrameGraph(FrameGraphConfig&& frameGraphConfig, FrameGraphDescription&& frameGraphDescription)
{
	UNREFERENCED_PARAMETER(frameGraphConfig);
	UNREFERENCED_PARAMETER(frameGraphDescription);
}