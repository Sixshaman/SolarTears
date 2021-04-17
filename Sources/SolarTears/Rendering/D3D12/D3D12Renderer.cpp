#include "D3D12Renderer.hpp"
#include "D3D12Memory.hpp"
#include "D3D12DeviceQueues.hpp"
#include "D3D12WorkerCommandLists.hpp"
#include "D3D12Shaders.hpp"
#include "D3D12DescriptorManager.hpp"
#include "Scene/D3D12Scene.hpp"
#include "Scene/D3D12SceneBuilder.hpp"
#include "Scene/D3D12SceneDescriptorCreator.hpp"
#include "FrameGraph/D3D12FrameGraphDescriptorCreator.hpp"
#include "FrameGraph/D3D12FrameGraphBuilder.hpp"
#include "../../Core/ThreadPool.hpp"

#include "FrameGraph/Passes/D3D12CopyImagePass.hpp"

D3D12::Renderer::Renderer(LoggerQueue* loggerQueue, FrameCounter* frameCounter, ThreadPool* threadPool): ::Renderer(loggerQueue), mFrameCounterRef(frameCounter), mThreadPoolRef(threadPool)
{
#if defined(DEBUG) || defined(_DEBUG)
	EnableDebugMode();
#endif

	CreateFactory();

	wil::com_ptr_nothrow<IDXGIAdapter4> adapter;
	CreateAdapter(adapter.put());

	CreateDevice(adapter.get());

	mDeviceFeatures = std::make_unique<DeviceFeatures>(mDevice.get());

	mDeviceQueues       = std::make_unique<DeviceQueues>(mDevice.get());
	mSwapChain          = std::make_unique<SwapChain>(mLoggingBoard);
	mWorkerCommandLists = std::make_unique<WorkerCommandLists>(mDevice.get(), threadPool->GetWorkerThreadCount());

	mMemoryAllocator   = std::make_unique<MemoryManager>(mLoggingBoard);
	mShaderManager     = std::make_unique<ShaderManager>(mLoggingBoard, mDevice.get());
	mDescriptorManager = std::make_unique<DescriptorManager>(mDevice.get());
}

D3D12::Renderer::~Renderer()
{
}

void D3D12::Renderer::AttachToWindow(Window* window)
{
	assert(mDevice != nullptr);
	mSwapChain->BindToWindow(mFactory.get(), mDeviceQueues.get(), window);

	CreateFrameGraph(window->GetWidth(), window->GetHeight());
}

void D3D12::Renderer::ResizeWindowBuffers(Window* window)
{
	mSwapChain->Resize(mDeviceQueues.get(), window);

	CreateFrameGraph(window->GetWidth(), window->GetHeight());
}

void D3D12::Renderer::InitScene(SceneDescription* sceneDescription)
{
	mDeviceQueues->AllQueuesWaitStrong();

	mScene = std::make_unique<D3D12::RenderableScene>(mFrameCounterRef);
	sceneDescription->BindRenderableComponent(mScene.get());

	D3D12::RenderableSceneBuilder sceneBuilder(mScene.get());
	sceneDescription->BuildRenderableComponent(&sceneBuilder);

	sceneBuilder.BakeSceneFirstPart(mDevice.get(), mMemoryAllocator.get());
	sceneBuilder.BakeSceneSecondPart(mDeviceQueues.get(), mWorkerCommandLists.get());

	SceneDescriptorCreator      sceneDescriptorCreator(mScene.get());
	FrameGraphDescriptorCreator frameGraphDescriptorCreator(mFrameGraph.get());
	mDescriptorManager->ValidateDescriptorHeaps(mDevice.get(), &sceneDescriptorCreator, &frameGraphDescriptorCreator, DescriptorManager::FLAG_FRAME_GRAPH_UNCHANGED);
}

void D3D12::Renderer::RenderScene()
{
	mSwapChain->Present();
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

void D3D12::Renderer::EnableDebugMode()
{
	wil::com_ptr_nothrow<ID3D12Debug1> debugController;
	THROW_IF_FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.put())));

	debugController->EnableDebugLayer();
	debugController->SetEnableGPUBasedValidation(true);
}

void D3D12::Renderer::CreateDevice(IDXGIAdapter4* adapter)
{
	DXGI_ADAPTER_DESC3 adapterDesc;
	THROW_IF_FAILED(adapter->GetDesc3(&adapterDesc));

	mLoggingBoard->PostLogMessage(std::wstring(L"GPU: ") + adapterDesc.Description);
	mLoggingBoard->PostLogMessage(L"");

	THROW_IF_FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(mDevice.put())));
}

void D3D12::Renderer::CreateFrameGraph(uint32_t viewportWidth, uint32_t viewportHeight)
{
	mFrameGraph.reset();

	FrameGraphConfig frameGraphConfig;
	frameGraphConfig.SetScreenSize((uint16_t)viewportWidth, (uint16_t)viewportHeight);

	mFrameGraph = std::make_unique<D3D12::FrameGraph>(frameGraphConfig);

	D3D12::FrameGraphBuilder frameGraphBuilder(mFrameGraph.get(), mScene.get(), mDeviceFeatures.get(), mShaderManager.get());

	//GBufferPass::Register(&frameGraphBuilder, "GBuffer");
	CopyImagePass::Register(&frameGraphBuilder, "CopyImage");

	//frameGraphBuilder.AssignSubresourceName("GBuffer",   GBufferPass::ColorBufferImageId, "ColorBuffer");
	frameGraphBuilder.AssignSubresourceName("CopyImage", CopyImagePass::SrcImageId,       "ColorBuffer");
	frameGraphBuilder.AssignSubresourceName("CopyImage", CopyImagePass::DstImageId,       "Backbuffer");

	frameGraphBuilder.AssignBackbufferName("Backbuffer");

	frameGraphBuilder.Build(mDevice.get(), mMemoryAllocator.get(), mSwapChain.get());

	SceneDescriptorCreator      sceneDescriptorCreator(mScene.get());
	FrameGraphDescriptorCreator frameGraphDescriptorCreator(mFrameGraph.get());
	mDescriptorManager->ValidateDescriptorHeaps(mDevice.get(), &sceneDescriptorCreator, &frameGraphDescriptorCreator, DescriptorManager::FLAG_SCENE_UNCHANGED);
}
