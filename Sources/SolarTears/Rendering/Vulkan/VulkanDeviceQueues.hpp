#pragma once

#include <vulkan/vulkan.h>

namespace VulkanCBindings
{
	class DeviceQueues
	{
	public:
		DeviceQueues(VkPhysicalDevice physicalDevice);

		void InitQueueHandles(VkDevice device);

		uint32_t GetGraphicsQueueFamilyIndex() const;
		uint32_t GetComputeQueueFamilyIndex()  const;
		uint32_t GetTransferQueueFamilyIndex() const;

		void GraphicsQueueWait() const;
		void ComputeQueueWait()  const;
		void TransferQueueWait() const;

		void GraphicsQueueSubmit(VkCommandBuffer commandBuffer) const;
		void ComputeQueueSubmit(VkCommandBuffer  commandBuffer) const;
		void TransferQueueSubmit(VkCommandBuffer commandBuffer) const;

		void GraphicsQueueSubmit(VkCommandBuffer commandBuffer, VkSemaphore signalSemaphore) const;
		void ComputeQueueSubmit(VkCommandBuffer  commandBuffer, VkSemaphore signalSemaphore) const;
		void TransferQueueSubmit(VkCommandBuffer commandBuffer, VkSemaphore signalSemaphore) const;

		void GraphicsQueueSubmit(VkCommandBuffer commandBuffer, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStageFlags) const;
		void ComputeQueueSubmit(VkCommandBuffer  commandBuffer, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStageFlags) const;
		void TransferQueueSubmit(VkCommandBuffer commandBuffer, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStageFlags) const;

		void GraphicsQueueSubmit(VkCommandBuffer* commandBuffers, size_t commandBufferCount) const;
		void ComputeQueueSubmit(VkCommandBuffer* commandBuffers,  size_t commandBufferCount) const;
		void TransferQueueSubmit(VkCommandBuffer* commandBuffers, size_t commandBufferCount) const;

		void GraphicsQueueSubmit(VkCommandBuffer* commandBuffers, size_t commandBufferCount, VkPipelineStageFlags waitStageFlags, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence signalFence) const;
		void ComputeQueueSubmit(VkCommandBuffer*  commandBuffers, size_t commandBufferCount, VkPipelineStageFlags waitStageFlags, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence signalFence) const;
		void TransferQueueSubmit(VkCommandBuffer* commandBuffers, size_t commandBufferCount, VkPipelineStageFlags waitStageFlags, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence signalFence) const;

	private:
		void FindDeviceQueueIndices(VkPhysicalDevice physicalDevice);

		void QueueSubmit(VkQueue queue, VkCommandBuffer* commandBuffers, uint32_t commandBufferCount)                                                                                                                    const;
		void QueueSubmit(VkQueue queue, VkCommandBuffer* commandBuffers, uint32_t commandBufferCount,                                                                 VkSemaphore signalSemaphore, VkFence signalFence)  const;
		void QueueSubmit(VkQueue queue, VkCommandBuffer* commandBuffers, uint32_t commandBufferCount, VkPipelineStageFlags waitStageFlags, VkSemaphore waitSemaphore,                              VkFence signalFence)  const;
		void QueueSubmit(VkQueue queue, VkCommandBuffer* commandBuffers, uint32_t commandBufferCount, VkPipelineStageFlags waitStageFlags, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence signalFence)  const;

	private:
		uint32_t mGraphicsQueueFamilyIndex;
		uint32_t mComputeQueueFamilyIndex;
		uint32_t mTransferQueueFamilyIndex;

		VkQueue mGraphicsQueue;
		VkQueue mComputeQueue;
		VkQueue mTransferQueue;
	};
}