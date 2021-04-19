#include "D3D12SwapChain.hpp"
#include "D3D12DeviceQueues.hpp"
#include <array>

D3D12::SwapChain::SwapChain(LoggerQueue* logger): mLogger(logger), mSwapChainFormat(DXGI_FORMAT_B8G8R8A8_UNORM), mSwapchainWidth(1), mSwapchainHeight(1)
{
}

D3D12::SwapChain::~SwapChain()
{
}

void D3D12::SwapChain::BindToWindow(IDXGIFactory4* factory, DeviceQueues* deviceQueues, Window* window)
{
	deviceQueues->AllQueuesWaitStrong();

	mSwapchain.reset();
	CreateSwapChain(factory, deviceQueues->GraphicsQueueHandle(), window);

	for(int i = 0; i < SwapchainImageCount; i++)
	{
		THROW_IF_FAILED(mSwapchain->GetBuffer(i, IID_PPV_ARGS(mSwapchainImages[i].put())));
	}
}

void D3D12::SwapChain::Resize(DeviceQueues* deviceQueues, Window* window)
{
	deviceQueues->AllQueuesWaitStrong();

	for(uint32_t i = 0; i < SwapchainImageCount; i++)
	{
		mSwapchainImages[i].reset();
	}

	std::array<IUnknown*, SwapchainImageCount> presentQueues;
	presentQueues.fill(deviceQueues->GraphicsQueueHandle());

	//TODO: mGPU?
	std::array<UINT, SwapchainImageCount> creationNodeMasks;
	creationNodeMasks.fill(0);

	THROW_IF_FAILED(mSwapchain->ResizeBuffers1(SwapchainImageCount, window->GetWidth(), window->GetHeight(), mSwapChainFormat, 0, creationNodeMasks.data(), presentQueues.data()));

	for(int i = 0; i < SwapchainImageCount; i++)
	{
		THROW_IF_FAILED(mSwapchain->GetBuffer(i, IID_PPV_ARGS(mSwapchainImages[i].put())));
	}
}

void D3D12::SwapChain::Present() const
{
	DXGI_PRESENT_PARAMETERS presentParameters;
	presentParameters.DirtyRectsCount = 0;
	presentParameters.pDirtyRects     = nullptr;
	presentParameters.pScrollRect     = nullptr;
	presentParameters.pScrollOffset   = nullptr;

	THROW_IF_FAILED(mSwapchain->Present1(0, 0, &presentParameters));
}

ID3D12Resource* D3D12::SwapChain::GetSwapchainImage(uint32_t index) const
{
	return mSwapchainImages[index].get();
}

ID3D12Resource* D3D12::SwapChain::GetCurrentImage() const
{
	UINT currentImageIndex = mSwapchain->GetCurrentBackBufferIndex();
	return mSwapchainImages[currentImageIndex].get();
}

DXGI_FORMAT D3D12::SwapChain::GetBackbufferFormat() const
{
	return mSwapChainFormat;
}

void D3D12::SwapChain::CreateSwapChain(IDXGIFactory4* factory, ID3D12CommandQueue* commandQueue, Window* window)
{
	mSwapChainFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

	mSwapchainWidth  = window->GetWidth();
	mSwapchainHeight = window->GetHeight();

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
	swapChainDesc.Width              = mSwapchainWidth;
	swapChainDesc.Height             = mSwapchainHeight;
	swapChainDesc.Format             = mSwapChainFormat;
	swapChainDesc.Stereo             = FALSE;
	swapChainDesc.SampleDesc.Count   = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount        = SwapchainImageCount;
	swapChainDesc.Scaling            = DXGI_SCALING_NONE;
	swapChainDesc.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode          = DXGI_ALPHA_MODE_IGNORE;
	swapChainDesc.Flags              = 0;

	wil::com_ptr_nothrow<IDXGISwapChain1> swapChain1;
	THROW_IF_FAILED(factory->CreateSwapChainForHwnd(commandQueue, window->NativeHandle(), &swapChainDesc, nullptr, nullptr, swapChain1.put()));

	THROW_IF_FAILED(swapChain1.query_to(mSwapchain.put()));
}