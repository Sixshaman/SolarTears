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
	deviceQueues->AllQueuesWait();

	mSwapchain.reset();
	CreateSwapChain(factory, deviceQueues->GraphicsQueueHandle(), window);

	for(int i = 0; i < SwapchainImageCount; i++)
	{
		THROW_IF_FAILED(mSwapchain->GetBuffer(i, IID_PPV_ARGS(mSwapchainImages[i].put())));
	}
}

void D3D12::SwapChain::Resize(DeviceQueues* deviceQueues, Window* window)
{
	deviceQueues->AllQueuesWait();

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

void D3D12::SwapChain::Present()
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

//void VulkanCBindings::SwapChain::AcquireImage(VkDevice device, uint32_t frameInFlightIndex)
//{
//	//TODO: mGPU?
//	VkAcquireNextImageInfoKHR acquireNextImageInfo;
//	acquireNextImageInfo.sType      = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR;
//	acquireNextImageInfo.pNext      = nullptr;
//	acquireNextImageInfo.swapchain  = mSwapchain;
//	acquireNextImageInfo.timeout    = 2000000000;
//	acquireNextImageInfo.semaphore  = mImageAcquiredSemaphores[frameInFlightIndex];
//	acquireNextImageInfo.fence      = nullptr;
//	acquireNextImageInfo.deviceMask = 0x01;
//	
//	ThrowIfFailed(vkAcquireNextImage2KHR(device, &acquireNextImageInfo, &mCurrentImageIndex));
//}

//VkSemaphore VulkanCBindings::SwapChain::GetImageAcquiredSemaphore(uint32_t frameInFlightIndex) const
//{
//	assert(frameInFlightIndex < SwapchainImageCount);
//
//	return mImageAcquiredSemaphores[frameInFlightIndex];
//}
//
//void VulkanCBindings::SwapChain::Present(VkSemaphore presentSemaphore)
//{
//	std::array presentSemaphores = {presentSemaphore};
//	std::array swapchains        = {mSwapchain};
//	std::array imageIndices      = {mCurrentImageIndex};
//
//	VkPresentInfoKHR presentInfo;
//	presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
//	presentInfo.pNext              = nullptr;
//	presentInfo.waitSemaphoreCount = (uint32_t)presentSemaphores.size();
//	presentInfo.pWaitSemaphores    = presentSemaphores.data();
//	presentInfo.swapchainCount     = (uint32_t)swapchains.size();
//	presentInfo.pSwapchains        = swapchains.data();
//	presentInfo.pImageIndices      = imageIndices.data();
//	presentInfo.pResults           = nullptr;
//
//	ThrowIfFailed(vkQueuePresentKHR(mPresentQueue, &presentInfo));
//}
//
//uint32_t VulkanCBindings::SwapChain::GetPresentQueueFamilyIndex() const
//{
//	return mPresentQueueFamilyIndex;
//}
//
//VkImage VulkanCBindings::SwapChain::GetSwapchainImage(uint32_t index) const
//{
//	assert(index < SwapchainImageCount);
//
//	return mSwapchainImages[index];
//}
//
//VkImage VulkanCBindings::SwapChain::GetCurrentImage() const
//{
//	return mSwapchainImages[mCurrentImageIndex];
//}
//
//VkFormat VulkanCBindings::SwapChain::GetBackbufferFormat() const
//{
//	return mSwapChainFormat;
//}
//
//bool VulkanCBindings::SwapChain::IsBackbufferHDR() const
//{
//	return (mSwapChainColorSpace == VK_COLOR_SPACE_HDR10_HLG_EXT) || (mSwapChainColorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT) || (mSwapChainColorSpace == VK_COLOR_SPACE_DOLBYVISION_EXT);
//}
//
//void VulkanCBindings::SwapChain::CreateSurface(VkInstance instance, Window* window)
//{
//	CreateSurface(instance, window->NativeHandle());
//}

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