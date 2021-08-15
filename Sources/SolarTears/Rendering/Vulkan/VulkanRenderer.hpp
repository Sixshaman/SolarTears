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
	class SceneDescriptorDatabase;

	class Renderer: public ::Renderer
	{
	public:
		Renderer(LoggerQueue* loggerQueue, FrameCounter* frameCounter, ThreadPool* threadPool);
		~Renderer();

		void AttachToWindow(Window* window)      override;
		void ResizeWindowBuffers(Window* window) override;

		BaseRenderableScene* InitScene(const RenderableSceneDescription& sceneDescription, const std::unordered_map<std::string_view, SceneObjectLocation>& sceneMeshInitialLocations, std::unordered_map<std::string_view, RenderableSceneObjectHandle>& outObjectHandles) override;
		void                 InitFrameGraph(FrameGraphConfig&& frameGraphConfig, FrameGraphDescription&& frameGraphDescription)                                                                                                                                             override;

		void Render() override;

	private:
		void InitInstance();
		void InitDebuggingEnvironment();
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

		std::unique_ptr<SwapChain>            mSwapChain;
		std::unique_ptr<DeviceQueues>         mDeviceQueues;
		std::unique_ptr<WorkerCommandBuffers> mCommandBuffers;

		std::unique_ptr<MemoryManager> mMemoryAllocator;

		std::unique_ptr<SceneDescriptorDatabase> mSceneDescriptorDatabase;

#if (defined(DEBUG) || defined(_DEBUG)) && defined(VK_EXT_debug_utils)
		VkDebugUtilsMessengerEXT mDebugMessenger;
#endif
	};
}