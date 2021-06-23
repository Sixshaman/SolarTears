#pragma once

#include "../Common/Renderer.hpp"
#include "../Common/RenderingUtils.hpp"
#include "VulkanUtils.hpp"
#include "VulkanInstanceParameters.hpp"
#include "VulkanDeviceParameters.hpp"
#include <unordered_set>
#include <memory>

class ThreadPool;
class FrameCounter;

namespace Vulkan
{
	class FunctionsLibrary;
	class WorkerCommandBuffers;
	class MemoryManager;
	class SwapChain;
	class DeviceQueues;
	class RenderableScene;
	class FrameGraph;
	class ShaderManager;
	class DescriptorManager;

	class Renderer: public ::Renderer
	{
	public:
		Renderer(LoggerQueue* loggerQueue, FrameCounter* frameCounter, ThreadPool* threadPool);
		~Renderer();

		void AttachToWindow(Window* window)      override;
		void ResizeWindowBuffers(Window* window) override;

		void InitScene(SceneDescription* sceneDescription) override;
		void RenderScene()                                 override;

		void InitFrameGraph(const FrameGraphConfig& frameGraphConfig) override;

	private:
		void InitInstance();
		void SelectPhysicalDevice(VkPhysicalDevice* outPhysicalDevice);
		void CreateLogicalDevice(VkPhysicalDevice physicalDevice, const std::unordered_set<uint32_t>& queueFamilies);
		void CreateFences();

	private:
		void InitializeSwapchainImages();

	private:
		ThreadPool*         mThreadPoolRef;
		const FrameCounter* mFrameCounterRef;

		VkInstance       mInstance;
		VkPhysicalDevice mPhysicalDevice;
		VkDevice         mDevice;

		VkFence mRenderFences[Utils::InFlightFrameCount];

		InstanceParameters mInstanceParameters;
		DeviceParameters   mDeviceParameters;

		//TODO: Stack allocator for heap-allocated class members
		std::unique_ptr<RenderableScene> mScene;
		std::unique_ptr<FrameGraph>      mFrameGraph;
		
		std::unique_ptr<FunctionsLibrary> mDynamicLibrary;

		std::unique_ptr<ShaderManager>     mShaderManager;
		std::unique_ptr<DescriptorManager> mDescriptorManager;

		std::unique_ptr<SwapChain>            mSwapChain;
		std::unique_ptr<DeviceQueues>         mDeviceQueues;
		std::unique_ptr<WorkerCommandBuffers> mCommandBuffers;

		std::unique_ptr<MemoryManager> mMemoryAllocator;
	};
}