#pragma once

#include <vulkan/vulkan.h>
#include <span>

namespace Vulkan
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

		void GraphicsQueueSubmit(std::span<VkCommandBuffer> commandBuffers) const;
		void ComputeQueueSubmit(std::span<VkCommandBuffer>  commandBuffers) const;
		void TransferQueueSubmit(std::span<VkCommandBuffer> commandBuffers) const;

		void GraphicsQueueSubmit(std::span<VkCommandBuffer> commandBuffers, std::span<VkPipelineStageFlags> waitStageFlags, std::span<VkSemaphore> waitSemaphores, VkSemaphore signalSemaphore, VkFence signalFence) const;
		void ComputeQueueSubmit(std::span<VkCommandBuffer> commandBuffers,  std::span<VkPipelineStageFlags> waitStageFlags, std::span<VkSemaphore> waitSemaphores, VkSemaphore signalSemaphore, VkFence signalFence) const;
		void TransferQueueSubmit(std::span<VkCommandBuffer> commandBuffers, std::span<VkPipelineStageFlags> waitStageFlags, std::span<VkSemaphore> waitSemaphores, VkSemaphore signalSemaphore, VkFence signalFence) const;

	private:
		void FindDeviceQueueIndices(VkPhysicalDevice physicalDevice);

		void QueueSubmit(VkQueue queue, std::span<VkCommandBuffer> commandBuffers)                                                                                                                                          const;
		void QueueSubmit(VkQueue queue, std::span<VkCommandBuffer> commandBuffers,                                                                                        VkSemaphore signalSemaphore, VkFence signalFence) const;
		void QueueSubmit(VkQueue queue, std::span<VkCommandBuffer> commandBuffers, std::span<VkPipelineStageFlags> waitStageFlags, std::span<VkSemaphore> waitSemaphores,                              VkFence signalFence) const;
		void QueueSubmit(VkQueue queue, std::span<VkCommandBuffer> commandBuffers, std::span<VkPipelineStageFlags> waitStageFlags, std::span<VkSemaphore> waitSemaphores, VkSemaphore signalSemaphore, VkFence signalFence) const;

	private:
		uint32_t mGraphicsQueueFamilyIndex;
		uint32_t mComputeQueueFamilyIndex;
		uint32_t mTransferQueueFamilyIndex;

		VkQueue mGraphicsQueue;
		VkQueue mComputeQueue;
		VkQueue mTransferQueue;
	};
}