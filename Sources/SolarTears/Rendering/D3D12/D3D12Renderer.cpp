#include "D3D12Renderer.hpp"

D3D12::Renderer::Renderer(LoggerQueue* loggerQueue, FrameCounter* frameCounter, ThreadPool* threadPool): ::Renderer(loggerQueue), mFrameCounterRef(frameCounter), mThreadPoolRef(threadPool)
{
}

D3D12::Renderer::~Renderer()
{
}

void D3D12::Renderer::AttachToWindow(Window* window)
{
	UNREFERENCED_PARAMETER(window);
}

void D3D12::Renderer::ResizeWindowBuffers(Window* window)
{
	UNREFERENCED_PARAMETER(window);
}

void D3D12::Renderer::InitScene(SceneDescription* sceneDescription)
{
	UNREFERENCED_PARAMETER(sceneDescription);
}

void D3D12::Renderer::RenderScene()
{
}
