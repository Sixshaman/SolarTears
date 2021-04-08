#pragma once

#include "../Renderer.hpp"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <unordered_set>
#include <memory>
#include <wil/com.h>

class ThreadPool;
class FrameCounter;

namespace D3D12
{
	class Renderer: public ::Renderer
	{
	public:
		Renderer(LoggerQueue* loggerQueue, FrameCounter* frameCounter, ThreadPool* threadPool);
		~Renderer();

		void AttachToWindow(Window* window)      override;
		void ResizeWindowBuffers(Window* window) override;

		void InitScene(SceneDescription* sceneDescription) override;
		void RenderScene()                                 override;

	private:
		ThreadPool*         mThreadPoolRef;
		const FrameCounter* mFrameCounterRef;

		wil::com_ptr<IDXGIFactory4> mFactory;
		wil::com_ptr<ID3D12Device>  mDevice;
	};
}