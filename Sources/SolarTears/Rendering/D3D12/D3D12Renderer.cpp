#include "D3D12Renderer.hpp"
#include "Scene/D3D12Scene.hpp"
#include "Scene/D3D12SceneBuilder.hpp"

D3D12::Renderer::Renderer(LoggerQueue* loggerQueue, FrameCounter* frameCounter, ThreadPool* threadPool): ::Renderer(loggerQueue), mFrameCounterRef(frameCounter), mThreadPoolRef(threadPool)
{
	CreateFactory();

	wil::com_ptr_nothrow<IDXGIAdapter4> adapter;
	CreateAdapter(adapter.put());

	CreateDevice(adapter.get());
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
	mScene = std::make_unique<D3D12::RenderableScene>(mFrameCounterRef);
	sceneDescription->BindRenderableComponent(mScene.get());

	D3D12::RenderableSceneBuilder sceneBuilder(mScene.get());
	sceneDescription->BuildRenderableComponent(&sceneBuilder);
}

void D3D12::Renderer::RenderScene()
{
}

void D3D12::Renderer::CreateFactory()
{
	UINT flags = 0;
#if defined(DEBUG) || defined(_DEBUG)
	flags = DXGI_CREATE_FACTORY_DEBUG;
#endif

	THROW_IF_FAILED(CreateDXGIFactory2(flags, IID_PPV_ARGS(mFactory.put())));
}

void D3D12::Renderer::CreateAdapter(IDXGIAdapter4** outAdapter)
{
	wil::com_ptr_nothrow<IDXGIAdapter1> adapter;
	THROW_IF_FAILED(mFactory->EnumAdapters1(0, adapter.put()));

	THROW_IF_FAILED(adapter.query_to(outAdapter));
}

void D3D12::Renderer::CreateDevice(IDXGIAdapter4* adapter)
{
	DXGI_ADAPTER_DESC3 adapterDesc;
	THROW_IF_FAILED(adapter->GetDesc3(&adapterDesc));

	mLoggingBoard->PostLogMessage(std::wstring(L"GPU: ") + adapterDesc.Description);
	mLoggingBoard->PostLogMessage(L"");

	THROW_IF_FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&mDevice)));
}