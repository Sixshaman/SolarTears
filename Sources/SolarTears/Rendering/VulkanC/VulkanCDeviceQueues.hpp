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

		void GraphicsQueueSubmit(VkCommandBuffer commandBuffer);
		void ComputeQueueSubmit(VkCommandBuffer  commandBuffer);
		void TransferQueueSubmit(VkCommandBuffer commandBuffer);

		void GraphicsQueueSubmit(VkCommandBuffer commandBuffer, VkSemaphore signalSemaphore);
		void ComputeQueueSubmit(VkCommandBuffer  commandBuffer, VkSemaphore signalSemaphore);
		void TransferQueueSubmit(VkCommandBuffer commandBuffer, VkSemaphore signalSemaphore);

		void GraphicsQueueSubmit(VkCommandBuffer commandBuffer, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStageFlags);
		void ComputeQueueSubmit(VkCommandBuffer  commandBuffer, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStageFlags);
		void TransferQueueSubmit(VkCommandBuffer commandBuffer, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStageFlags);

		void GraphicsQueueSubmit(VkCommandBuffer* commandBuffers, size_t commandBufferCount);
		void ComputeQueueSubmit(VkCommandBuffer* commandBuffers,  size_t commandBufferCount);
		void TransferQueueSubmit(VkCommandBuffer* commandBuffers, size_t commandBufferCount);

		void GraphicsQueueSubmit(VkCommandBuffer* commandBuffers, size_t commandBufferCount, VkPipelineStageFlags waitStageFlags, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence signalFence);
		void ComputeQueueSubmit(VkCommandBuffer*  commandBuffers, size_t commandBufferCount, VkPipelineStageFlags waitStageFlags, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence signalFence);
		void TransferQueueSubmit(VkCommandBuffer* commandBuffers, size_t commandBufferCount, VkPipelineStageFlags waitStageFlags, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence signalFence);

	private:
		void FindDeviceQueueIndices(VkPhysicalDevice physicalDevice);

		void QueueSubmit(VkQueue queue, VkCommandBuffer* commandBuffers, uint32_t commandBufferCount);
		void QueueSubmit(VkQueue queue, VkCommandBuffer* commandBuffers, uint32_t commandBufferCount,                                                                 VkSemaphore signalSemaphore, VkFence signalFence);
		void QueueSubmit(VkQueue queue, VkCommandBuffer* commandBuffers, uint32_t commandBufferCount, VkPipelineStageFlags waitStageFlags, VkSemaphore waitSemaphore,                              VkFence signalFence);
		void QueueSubmit(VkQueue queue, VkCommandBuffer* commandBuffers, uint32_t commandBufferCount, VkPipelineStageFlags waitStageFlags, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence signalFence);

	private:
		uint32_t mGraphicsQueueFamilyIndex;
		uint32_t mComputeQueueFamilyIndex;
		uint32_t mTransferQueueFamilyIndex;

		VkQueue mGraphicsQueue;
		VkQueue mComputeQueue;
		VkQueue mTransferQueue;
	};
}