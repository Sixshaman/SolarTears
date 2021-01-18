#pragma once

#include "../Renderer.hpp"
#include "VulkanCUtils.hpp"
#include "VulkanCInstanceParameters.hpp"
#include "VulkanCDeviceParameters.hpp"
#include <unordered_set>
#include <memory>

class ThreadPool;
namespace VulkanCBindings
{
	class FunctionsLibrary;
	class WorkerCommandBuffers;
	class MemoryManager;
	class SwapChain;
	class RenderableScene;
	class FrameGraph;
	class ShaderManager;

	class Renderer: public ::Renderer
	{
	public:
		Renderer(LoggerQueue* loggerQueue, ThreadPool* threadPool);
		~Renderer();

		void AttachToWindow(Window* window)      override;
		void ResizeWindowBuffers(Window* window) override;

		void InitScene(SceneDescription* scene) override;
		void RenderScene()                      override;
		
		uint64_t GetFrameNumber() const override;

	private:
		void InitInstance();
		void SelectPhysicalDevice(VkPhysicalDevice* outPhysicalDevice);
		void CreateLogicalDevice(VkPhysicalDevice physicalDevice, const std::unordered_set<uint32_t>& queueFamilies);

		void FindDeviceQueueIndices(VkPhysicalDevice physicalDevice, uint32_t* outGraphicsQueueIndex, uint32_t* outComputeQueueIndex, uint32_t* outTransferQueueIndex);

		void GetDeviceQueues(uint32_t graphicsIndex, uint32_t computeIndex, uint32_t transferIndex);

		void CreateCommandBuffersAndPools();
		void CreateFences();

	private:
		void InitializeSwapchainImages();

		void CreateFrameGraph();

	private:
		ThreadPool* mThreadPool;

		VkInstance       mInstance;
		VkPhysicalDevice mPhysicalDevice;
		VkDevice         mDevice;

		uint32_t mGraphicsQueueFamilyIndex;
		uint32_t mComputeQueueFamilyIndex;
		uint32_t mTransferQueueFamilyIndex;

		VkQueue mGraphicsQueue;
		VkQueue mComputeQueue;
		VkQueue mTransferQueue;

		VkCommandPool mMainGraphicsCommandPools[VulkanUtils::InFlightFrameCount];
		VkCommandPool mMainComputeCommandPools[VulkanUtils::InFlightFrameCount];
		VkCommandPool mMainTransferCommandPools[VulkanUtils::InFlightFrameCount];

		VkCommandBuffer mMainGraphicsCommandBuffers[VulkanUtils::InFlightFrameCount];
		VkCommandBuffer mMainComputeCommandBuffers[VulkanUtils::InFlightFrameCount];
		VkCommandBuffer mMainTransferCommandBuffers[VulkanUtils::InFlightFrameCount];

		VkFence mRenderFences[VulkanUtils::InFlightFrameCount];

		uint64_t mCurrentFrameIndex;
		uint32_t mThreadCount;

		VulkanCBindings::InstanceParameters mInstanceParameters;
		VulkanCBindings::DeviceParameters   mDeviceParameters;

		//TODO: Stack allocator for heap-allocated class members
		std::unique_ptr<RenderableScene> mScene;
		std::unique_ptr<FrameGraph>      mFrameGraph;
		
		std::unique_ptr<FunctionsLibrary> mDynamicLibrary;

		std::unique_ptr<ShaderManager> mShaderManager;

		std::unique_ptr<SwapChain>            mSwapChain;
		std::unique_ptr<WorkerCommandBuffers> mCommandBuffers;

		std::unique_ptr<MemoryManager> mMemoryAllocator;
	};
}