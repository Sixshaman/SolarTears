#include "D3D12Renderer.hpp"
#include "D3D12Memory.hpp"
#include "D3D12DeviceQueues.hpp"
#include "D3D12WorkerCommandLists.hpp"
#include "D3D12Shaders.hpp"
#include "D3D12SrvDescriptorManager.hpp"
#include "Scene/D3D12Scene.hpp"
#include "Scene/D3D12SceneBuilder.hpp"
#include "D3D12DescriptorCreator.hpp"
#include "FrameGraph/D3D12FrameGraphBuilder.hpp"
#include "../../Core/ThreadPool.hpp"
#include "../Common/RenderingUtils.hpp"

#include "FrameGraph/Passes/D3D12GBufferPass.hpp"
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
	mShaderManager     = std::make_unique<ShaderManager>(mLoggingBoard);
	mDescriptorManager = std::make_unique<SrvDescriptorManager>();

	mFrameGraphicsFenceValues.fill(0);
}

D3D12::Renderer::~Renderer()
{
	mDeviceQueues->AllQueuesWaitStrong();
}

void D3D12::Renderer::AttachToWindow(Window* window)
{
	assert(mDevice != nullptr);
	mSwapChain->BindToWindow(mFactory.get(), mDeviceQueues.get(), window);
}

void D3D12::Renderer::ResizeWindowBuffers(Window* window)
{
	mSwapChain->Resize(mDeviceQueues.get(), window);
}

BaseRenderableScene* D3D12::Renderer::InitScene(const RenderableSceneDescription& sceneDescription, const std::unordered_map<std::string_view, SceneObjectLocation>& sceneMeshInitialLocations, std::unordered_map<std::string_view, RenderableSceneObjectHandle>& outObjectHandles)
{
	mDeviceQueues->AllQueuesWaitStrong();

	mScene = std::make_unique<D3D12::RenderableScene>();
	D3D12::RenderableSceneBuilder sceneBuilder(mDevice.get(), mScene.get(), mMemoryAllocator.get(), mDeviceQueues.get(), mWorkerCommandLists.get());

	sceneBuilder.Build(sceneDescription, sceneMeshInitialLocations, outObjectHandles);

	RecreateSceneAndFrameGraphDescriptors();
	return mScene.get();
}

void D3D12::Renderer::InitFrameGraph(FrameGraphConfig&& frameGraphConfig, FrameGraphDescription&& frameGraphDescription)
{
	mDeviceQueues->AllQueuesWaitStrong();

	mFrameGraph = std::make_unique<D3D12::FrameGraph>(std::move(frameGraphConfig), mWorkerCommandLists.get(), mDescriptorManager.get(), mDeviceQueues.get());
	D3D12::FrameGraphBuilder frameGraphBuilder(mFrameGraph.get(), mSwapChain.get());

	D3D12::FrameGraphBuildInfo buildInfo;
	buildInfo.Device          = mDevice.get();
	buildInfo.ShaderMgr       = mShaderManager.get();
	buildInfo.MemoryAllocator = mMemoryAllocator.get();

	frameGraphBuilder.Build(std::move(frameGraphDescription), buildInfo);

	RecreateSceneAndFrameGraphDescriptors();
}

void D3D12::Renderer::Render()
{
	const uint32_t currentFrameResourceIndex = mFrameCounterRef->GetFrameCount() % Utils::InFlightFrameCount;

	mDeviceQueues->GraphicsQueueCpuWait(mFrameGraphicsFenceValues[currentFrameResourceIndex]);

	mFrameGraph->Traverse(mThreadPoolRef, mScene.get(), currentFrameResourceIndex, mSwapChain->GetCurrentImageIndex());

	mFrameGraphicsFenceValues[currentFrameResourceIndex] = mDeviceQueues->GraphicsFenceSignal();

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

void D3D12::Renderer::RecreateSceneAndFrameGraphDescriptors()
{
	DescriptorCreator descriptorCreator(mFrameGraph.get(), mScene.get());
	UINT descriptorCountNeeded = descriptorCreator.GetDescriptorCountNeeded();

	//Recreate descriptor heaps if needed
	mDescriptorManager->ValidateDescriptorHeaps(mDevice.get(), descriptorCountNeeded);

	ID3D12DescriptorHeap* descriptorHeap = mDescriptorManager->GetDescriptorHeap();
	descriptorCreator.RecreateDescriptors(mDevice.get(), descriptorHeap->GetCPUDescriptorHandleForHeapStart(), descriptorHeap->GetGPUDescriptorHandleForHeapStart());
}
