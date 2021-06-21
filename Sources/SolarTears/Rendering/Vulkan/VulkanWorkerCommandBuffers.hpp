#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "VulkanUtils.hpp"

namespace Vulkan
{
	class DeviceQueues;
	class SwapChain;

	class WorkerCommandBuffers
	{
	public:
		WorkerCommandBuffers(VkDevice device, uint32_t workerThreadCount, const DeviceQueues* deviceQueues, const SwapChain* swapChain);
		~WorkerCommandBuffers();

		uint32_t GetWorkerThreadCount() const;
	
		VkCommandPool GetMainThreadGraphicsCommandPool(uint32_t frameIndex) const;
		VkCommandPool GetMainThreadComputeCommandPool(uint32_t frameIndex)  const;
		VkCommandPool GetMainThreadTransferCommandPool(uint32_t frameIndex) const;

		VkCommandBuffer GetMainThreadGraphicsCommandBuffer(uint32_t frameIndex) const;
		VkCommandBuffer GetMainThreadComputeCommandBuffer(uint32_t frameIndex)  const;
		VkCommandBuffer GetMainThreadTransferCommandBuffer(uint32_t frameIndex) const;

		VkCommandPool GetThreadGraphicsCommandPool(uint32_t threadIndex, uint32_t frameIndex) const;
		VkCommandPool GetThreadComputeCommandPool(uint32_t threadIndex, uint32_t frameIndex)  const;
		VkCommandPool GetThreadTransferCommandPool(uint32_t threadIndex, uint32_t frameIndex) const;

		VkCommandBuffer GetThreadGraphicsCommandBuffer(uint32_t threadIndex, uint32_t frameIndex) const;
		VkCommandBuffer GetThreadComputeCommandBuffer(uint32_t threadIndex, uint32_t frameIndex)  const;
		VkCommandBuffer GetThreadTransferCommandBuffer(uint32_t threadIndex, uint32_t frameIndex) const;

		VkCommandPool GetMainThreadAcquireCommandPool(uint32_t frameIndex) const;
		VkCommandPool GetMainThreadPresentCommandPool(uint32_t frameIndex) const;

		VkCommandBuffer GetMainThreadAcquireCommandBuffer(uint32_t frameIndex) const;
		VkCommandBuffer GetMainThreadPresentCommandBuffer(uint32_t frameIndex) const;

	private:
		void InitCommandBuffers(uint32_t graphicsQueueFamilyIndex, uint32_t computeQueueFamilyIndex, uint32_t transferQueueFamilyIndex);
		void InitPresentQueueCommandBuffers(uint32_t presentQueueFamilyIndex);

		VkCommandPool   CreateCommandPool(uint32_t queueFamilyIndex);
		VkCommandBuffer CreateCommandBuffer(VkCommandPool commandPool);

	private:
		const VkDevice mDeviceRef;

		std::vector<VkCommandPool>   mCommandPools;
		std::vector<VkCommandBuffer> mCommandBuffers;

		uint32_t mWorkerThreadCount;
		uint32_t mSeparateQueueCount;

		uint32_t mCommandBufferIndexForGraphics;
		uint32_t mCommandBufferIndexForCompute;
		uint32_t mCommandBufferIndexForTransfer;
	};
}