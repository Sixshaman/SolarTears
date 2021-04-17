#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wil/com.h>
#include "D3D12Utils.hpp"
#include "../../Core/Window.hpp"

namespace D3D12
{
	class DeviceQueues;

	class SwapChain
	{
	public:
		static constexpr uint32_t SwapchainImageCount = D3D12Utils::InFlightFrameCount;

	public:
		SwapChain(LoggerQueue* logger);
		~SwapChain();

		void BindToWindow(IDXGIFactory4* factory, DeviceQueues* deviceQueues, Window* window);
		void Resize(DeviceQueues* deviceQueues, Window* window);

		void Present();

		ID3D12Resource* GetSwapchainImage(uint32_t index)  const;
		ID3D12Resource* GetCurrentImage()                  const;

		DXGI_FORMAT GetBackbufferFormat() const;

	private:
		void CreateSwapChain(IDXGIFactory4* factory, ID3D12CommandQueue* commandQueue, Window* window);

	private:
		LoggerQueue* mLogger;

		wil::com_ptr_nothrow<IDXGISwapChain4> mSwapchain;

		uint32_t mSwapchainWidth;
		uint32_t mSwapchainHeight;

		wil::com_ptr_nothrow<ID3D12Resource1> mSwapchainImages[SwapchainImageCount];

		DXGI_FORMAT mSwapChainFormat;
	};
}