#pragma once

#include "../Renderer.hpp"
#include "VulkanCUtils.hpp"
#include "VulkanCInstanceParameters.hpp"
#include "VulkanCDeviceParameters.hpp"
#include <unordered_set>
#include <memory>

class ThreadPool;
class FrameCounter;

namespace VulkanCBindings
{
	class FunctionsLibrary;
	class WorkerCommandBuffers;
	class MemoryManager;
	class SwapChain;
	class DeviceQueues;
	class RenderableScene;
	class FrameGraph;
	class ShaderManager;

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
		void InitInstance();
		void SelectPhysicalDevice(VkPhysicalDevice* outPhysicalDevice);
		void CreateLogicalDevice(VkPhysicalDevice physicalDevice, const std::unordered_set<uint32_t>& queueFamilies);
		void CreateFences();

	private:
		void InitializeSwapchainImages();

		void CreateFrameGraph(uint32_t viewportWidth, uint32_t viewportHeight);

	private:
		const ThreadPool*   mThreadPoolRef;
		const FrameCounter* mFrameCounterRef;

		VkInstance       mInstance;
		VkPhysicalDevice mPhysicalDevice;
		VkDevice         mDevice;

		VkFence mRenderFences[VulkanUtils::InFlightFrameCount];

		VulkanCBindings::InstanceParameters mInstanceParameters;
		VulkanCBindings::DeviceParameters   mDeviceParameters;

		//TODO: Stack allocator for heap-allocated class members
		std::unique_ptr<RenderableScene> mScene;
		std::unique_ptr<FrameGraph>      mFrameGraph;
		
		std::unique_ptr<FunctionsLibrary> mDynamicLibrary;

		std::unique_ptr<ShaderManager> mShaderManager;

		std::unique_ptr<SwapChain>            mSwapChain;
		std::unique_ptr<DeviceQueues>         mDeviceQueues;
		std::unique_ptr<WorkerCommandBuffers> mCommandBuffers;

		std::unique_ptr<MemoryManager> mMemoryAllocator;
	};
}