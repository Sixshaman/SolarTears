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
	class RenderableScene;

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
		void CreateFactory();
		void CreateAdapter(IDXGIAdapter4** outAdapter);

		void CreateDevice(IDXGIAdapter4* adapter);

	private:
		ThreadPool*         mThreadPoolRef;
		const FrameCounter* mFrameCounterRef;

		wil::com_ptr_nothrow<IDXGIFactory4> mFactory;
		wil::com_ptr_nothrow<ID3D12Device8> mDevice;

		std::unique_ptr<RenderableScene> mScene;
	};
}