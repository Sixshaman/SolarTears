#pragma once

#include "../Common/Renderer.hpp"
#include "../Common/RenderingUtils.hpp"
#include "D3D12SwapChain.hpp"
#include "D3D12DeviceFeatures.hpp"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <memory>
#include <array>
#include <wil/com.h>

class ThreadPool;
class FrameCounter;

namespace D3D12
{
	class RenderableScene;
	class FrameGraph;
	class MemoryManager;
	class ShaderManager;
	class SrvDescriptorManager;
	class DeviceQueues;
	class WorkerCommandLists;

	class Renderer: public ::Renderer
	{
	public:
		Renderer(LoggerQueue* loggerQueue, FrameCounter* frameCounter, ThreadPool* threadPool);
		~Renderer();

		void AttachToWindow(Window* window)      override;
		void ResizeWindowBuffers(Window* window) override;

		void InitScene(const RenderableSceneDescription& sceneDescription)                                      override;
		void InitFrameGraph(FrameGraphConfig&& frameGraphConfig, FrameGraphDescription&& frameGraphDescription) override;

		void Render() override;

	private:
		void CreateFactory();
		void CreateAdapter(IDXGIAdapter4** outAdapter);

		void EnableDebugMode();
		void CreateDevice(IDXGIAdapter4* adapter);

	private:
		ThreadPool*         mThreadPoolRef;
		const FrameCounter* mFrameCounterRef;

		wil::com_ptr_nothrow<IDXGIFactory4> mFactory;
		wil::com_ptr_nothrow<ID3D12Device8> mDevice;

		std::unique_ptr<RenderableScene> mScene;
		std::unique_ptr<FrameGraph>      mFrameGraph;

		std::unique_ptr<SwapChain>          mSwapChain;
		std::unique_ptr<DeviceQueues>       mDeviceQueues;
		std::unique_ptr<WorkerCommandLists> mWorkerCommandLists;

		std::unique_ptr<DeviceFeatures> mDeviceFeatures;

		std::unique_ptr<MemoryManager>        mMemoryAllocator;
		std::unique_ptr<ShaderManager>        mShaderManager;
		std::unique_ptr<SrvDescriptorManager> mDescriptorManager;

		std::array<UINT64, Utils::InFlightFrameCount> mFrameGraphicsFenceValues;
	};
}