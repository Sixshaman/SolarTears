#include "VulkanDeviceQueues.hpp"
#include "VulkanFunctions.hpp"
#include "VulkanUtils.hpp"
#include <cassert>
#include <vector>
#include <array>

Vulkan::DeviceQueues::DeviceQueues(VkPhysicalDevice physicalDevice)
{
	mGraphicsQueue = VK_NULL_HANDLE;
	mComputeQueue  = VK_NULL_HANDLE;
	mTransferQueue = VK_NULL_HANDLE;

	FindDeviceQueueIndices(physicalDevice);
}

void Vulkan::DeviceQueues::InitQueueHandles(VkDevice device)
{
	VkDeviceQueueInfo2 graphicsQueueInfo;
	graphicsQueueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
	graphicsQueueInfo.pNext            = nullptr;
	graphicsQueueInfo.flags            = 0; //I still don't use ANY queue priorities
	graphicsQueueInfo.queueIndex       = 0; //Use only one queue per family
	graphicsQueueInfo.queueFamilyIndex = mGraphicsQueueFamilyIndex;

	vkGetDeviceQueue2(device, &graphicsQueueInfo, &mGraphicsQueue);
	if(mGraphicsQueue == VK_NULL_HANDLE) //Some older AMD cards return NULL on vkGetDeviceQueue2
	{
		vkGetDeviceQueue(device, mGraphicsQueueFamilyIndex, 0, &mGraphicsQueue);
	}


	VkDeviceQueueInfo2 computeQueueInfo;
	computeQueueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
	computeQueueInfo.pNext            = nullptr;
	computeQueueInfo.flags            = 0; //I still don't use ANY queue priorities
	computeQueueInfo.queueIndex       = 0; //Use only one queue per family
	computeQueueInfo.queueFamilyIndex = mComputeQueueFamilyIndex;

	vkGetDeviceQueue2(device, &computeQueueInfo, &mComputeQueue);
	if(mComputeQueue == VK_NULL_HANDLE) //Some older AMD cards return NULL on vkGetDeviceQueue2
	{
		vkGetDeviceQueue(device, mComputeQueueFamilyIndex, 0, &mComputeQueue);
	}


	VkDeviceQueueInfo2 transferQueueInfo;
	transferQueueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
	transferQueueInfo.pNext            = nullptr;
	transferQueueInfo.flags            = 0; //I still don't use ANY queue priorities
	transferQueueInfo.queueIndex       = 0; //Use only one queue per family
	transferQueueInfo.queueFamilyIndex = mTransferQueueFamilyIndex;

	vkGetDeviceQueue2(device, &transferQueueInfo, &mTransferQueue);
	if(mTransferQueue == VK_NULL_HANDLE) //Some older AMD cards return NULL on vkGetDeviceQueue2
	{
		vkGetDeviceQueue(device, mTransferQueueFamilyIndex, 0, &mTransferQueue);
	}
}

uint32_t Vulkan::DeviceQueues::GetGraphicsQueueFamilyIndex() const
{
	return mGraphicsQueueFamilyIndex;
}

uint32_t Vulkan::DeviceQueues::GetComputeQueueFamilyIndex() const
{
	return mComputeQueueFamilyIndex;
}

uint32_t Vulkan::DeviceQueues::GetTransferQueueFamilyIndex() const
{
	return mTransferQueueFamilyIndex;
}

void Vulkan::DeviceQueues::GraphicsQueueWait() const
{
	ThrowIfFailed(vkQueueWaitIdle(mGraphicsQueue));
}

void Vulkan::DeviceQueues::ComputeQueueWait() const
{
	ThrowIfFailed(vkQueueWaitIdle(mComputeQueue));
}

void Vulkan::DeviceQueues::TransferQueueWait() const
{
	ThrowIfFailed(vkQueueWaitIdle(mTransferQueue));
}

void Vulkan::DeviceQueues::GraphicsQueueSubmit(VkCommandBuffer commandBuffer) const
{
	std::array commandBuffersToExecute = {commandBuffer};
	GraphicsQueueSubmit(commandBuffersToExecute);
}

void Vulkan::DeviceQueues::ComputeQueueSubmit(VkCommandBuffer commandBuffer) const
{
	std::array commandBuffersToExecute = {commandBuffer};
	ComputeQueueSubmit(commandBuffersToExecute);
}

void Vulkan::DeviceQueues::TransferQueueSubmit(VkCommandBuffer commandBuffer) const
{
	std::array commandBuffersToExecute = {commandBuffer};
	TransferQueueSubmit(commandBuffersToExecute);
}

void Vulkan::DeviceQueues::GraphicsQueueSubmit(VkCommandBuffer commandBuffer, VkSemaphore signalSemaphore) const
{
	std::array commandBuffersToExecute = {commandBuffer};
	QueueSubmit(mGraphicsQueue, commandBuffersToExecute, signalSemaphore, nullptr);
}

void Vulkan::DeviceQueues::ComputeQueueSubmit(VkCommandBuffer commandBuffer, VkSemaphore signalSemaphore) const
{
	std::array commandBuffersToExecute = {commandBuffer};
	QueueSubmit(mComputeQueue, commandBuffersToExecute, signalSemaphore, nullptr);
}

void Vulkan::DeviceQueues::TransferQueueSubmit(VkCommandBuffer commandBuffer, VkSemaphore signalSemaphore) const
{
	std::array commandBuffersToExecute = {commandBuffer};
	QueueSubmit(mTransferQueue, commandBuffersToExecute, signalSemaphore, nullptr);
}

void Vulkan::DeviceQueues::GraphicsQueueSubmit(VkCommandBuffer commandBuffer, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStageFlags) const
{
	std::array commandBuffersToExecute = {commandBuffer};
	std::array semaphoresToWait        = {waitSemaphore};
	std::array stagesToWait            = {waitStageFlags};
	QueueSubmit(mGraphicsQueue, commandBuffersToExecute, stagesToWait, semaphoresToWait, nullptr);
}

void Vulkan::DeviceQueues::ComputeQueueSubmit(VkCommandBuffer commandBuffer, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStageFlags) const
{
	std::array commandBuffersToExecute = {commandBuffer};
	std::array semaphoresToWait        = {waitSemaphore};
	std::array stagesToWait            = {waitStageFlags};
	QueueSubmit(mComputeQueue, commandBuffersToExecute, stagesToWait, semaphoresToWait, nullptr);
}

void Vulkan::DeviceQueues::TransferQueueSubmit(VkCommandBuffer commandBuffer, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStageFlags) const
{
	std::array commandBuffersToExecute = {commandBuffer};
	std::array semaphoresToWait        = {waitSemaphore};
	std::array stagesToWait            = {waitStageFlags};
	QueueSubmit(mTransferQueue, commandBuffersToExecute, stagesToWait, semaphoresToWait, nullptr);
}

void Vulkan::DeviceQueues::GraphicsQueueSubmit(std::span<VkCommandBuffer> commandBuffers) const
{
	QueueSubmit(mGraphicsQueue, commandBuffers);
}

void Vulkan::DeviceQueues::ComputeQueueSubmit(std::span<VkCommandBuffer> commandBuffers) const
{
	QueueSubmit(mComputeQueue, commandBuffers);
}

void Vulkan::DeviceQueues::TransferQueueSubmit(std::span<VkCommandBuffer> commandBuffers) const
{
	QueueSubmit(mTransferQueue, commandBuffers);
}

void Vulkan::DeviceQueues::GraphicsQueueSubmit(std::span<VkCommandBuffer> commandBuffers, std::span<VkPipelineStageFlags> waitStageFlags, std::span<VkSemaphore> waitSemaphores, VkSemaphore signalSemaphore, VkFence signalFence) const
{
	QueueSubmit(mGraphicsQueue, commandBuffers, waitStageFlags, waitSemaphores, signalSemaphore, signalFence);
}

void Vulkan::DeviceQueues::ComputeQueueSubmit(std::span<VkCommandBuffer> commandBuffers, std::span<VkPipelineStageFlags> waitStageFlags, std::span<VkSemaphore> waitSemaphores, VkSemaphore signalSemaphore, VkFence signalFence) const
{
	QueueSubmit(mComputeQueue, commandBuffers, waitStageFlags, waitSemaphores, signalSemaphore, signalFence);
}

void Vulkan::DeviceQueues::TransferQueueSubmit(std::span<VkCommandBuffer> commandBuffers, std::span<VkPipelineStageFlags> waitStageFlags, std::span<VkSemaphore> waitSemaphores, VkSemaphore signalSemaphore, VkFence signalFence) const
{
	QueueSubmit(mTransferQueue, commandBuffers, waitStageFlags, waitSemaphores, signalSemaphore, signalFence);
}

void Vulkan::DeviceQueues::FindDeviceQueueIndices(VkPhysicalDevice physicalDevice)
{
	//Use dedicated queues
	uint32_t graphicsQueueFamilyIndex = (uint32_t)(-1);
	uint32_t computeQueueFamilyIndex  = (uint32_t)(-1);
	uint32_t transferQueueFamilyIndex = (uint32_t)(-1);

	std::vector<VkQueueFamilyProperties> queueFamiliesProperties;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

	queueFamiliesProperties.resize(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamiliesProperties.data());
	for(uint32_t queueFamilyIndex = 0; queueFamilyIndex < (uint32_t)queueFamiliesProperties.size(); queueFamilyIndex++)
	{
		//First found graphics queue
		if((graphicsQueueFamilyIndex == (uint32_t)(-1)) && ((queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0))
		{
			graphicsQueueFamilyIndex = queueFamilyIndex;
		}
		
		//First found dedicated compute queue
		if((computeQueueFamilyIndex == (uint32_t)(-1)) && ((queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) && ((queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
		{
			computeQueueFamilyIndex = queueFamilyIndex;
		}

		//First found dedicated transfer queue
		if((transferQueueFamilyIndex == (uint32_t)(-1)) && ((queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_TRANSFER_BIT) != 0) && ((queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
		{
			transferQueueFamilyIndex = queueFamilyIndex;
		}
	}

	//Some of the queues are not found, use non-dedicated ones instead
	if(computeQueueFamilyIndex == (uint32_t)(-1))
	{
		computeQueueFamilyIndex = graphicsQueueFamilyIndex;
	}

	if(transferQueueFamilyIndex == (uint32_t)(-1))
	{
		transferQueueFamilyIndex = computeQueueFamilyIndex;
	}

	assert(graphicsQueueFamilyIndex != (uint32_t)(-1));
	assert(computeQueueFamilyIndex  != (uint32_t)(-1));
	assert(transferQueueFamilyIndex != (uint32_t)(-1));

	mGraphicsQueueFamilyIndex = graphicsQueueFamilyIndex;
	mComputeQueueFamilyIndex  = computeQueueFamilyIndex;
	mTransferQueueFamilyIndex = transferQueueFamilyIndex;
}

void Vulkan::DeviceQueues::QueueSubmit(VkQueue queue, std::span<VkCommandBuffer> commandBuffers) const
{
	//TODO: mGPU?
	//TODO: Performance query?
	VkSubmitInfo queueSubmitInfo;
	queueSubmitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	queueSubmitInfo.pNext                = nullptr;
	queueSubmitInfo.waitSemaphoreCount   = 0;
	queueSubmitInfo.pWaitSemaphores      = nullptr;
	queueSubmitInfo.pWaitDstStageMask    = nullptr;
	queueSubmitInfo.commandBufferCount   = (uint32_t)commandBuffers.size();
	queueSubmitInfo.pCommandBuffers      = commandBuffers.data();
	queueSubmitInfo.signalSemaphoreCount = 0;
	queueSubmitInfo.pSignalSemaphores    = nullptr;

	std::array submitInfos = {queueSubmitInfo};
	ThrowIfFailed(vkQueueSubmit(queue, (uint32_t)submitInfos.size(), submitInfos.data(), nullptr));
}

void Vulkan::DeviceQueues::QueueSubmit(VkQueue queue, std::span<VkCommandBuffer> commandBuffers, VkSemaphore signalSemaphore, VkFence signalFence) const
{
	std::array signalSemaphores = {signalSemaphore};

	//TODO: mGPU?
	//TODO: Performance query?
	VkSubmitInfo queueSubmitInfo;
	queueSubmitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	queueSubmitInfo.pNext                = nullptr;
	queueSubmitInfo.waitSemaphoreCount   = 0;
	queueSubmitInfo.pWaitSemaphores      = nullptr;
	queueSubmitInfo.pWaitDstStageMask    = nullptr;
	queueSubmitInfo.commandBufferCount   = (uint32_t)commandBuffers.size();
	queueSubmitInfo.pCommandBuffers      = commandBuffers.data();
	queueSubmitInfo.signalSemaphoreCount = (uint32_t)signalSemaphores.size();
	queueSubmitInfo.pSignalSemaphores    = signalSemaphores.data();

	std::array submitInfos = {queueSubmitInfo};
	ThrowIfFailed(vkQueueSubmit(queue, (uint32_t)submitInfos.size(), submitInfos.data(), signalFence));
}

void Vulkan::DeviceQueues::QueueSubmit(VkQueue queue, std::span<VkCommandBuffer> commandBuffers, std::span<VkPipelineStageFlags> waitStageFlags, std::span<VkSemaphore> waitSemaphores, VkFence signalFence) const
{
	assert(waitStageFlags.size() == waitSemaphores.size());

	//TODO: mGPU?
	//TODO: Performance query?
	VkSubmitInfo queueSubmitInfo;
	queueSubmitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	queueSubmitInfo.pNext                = nullptr;
	queueSubmitInfo.waitSemaphoreCount   = (uint32_t)waitSemaphores.size();
	queueSubmitInfo.pWaitSemaphores      = waitSemaphores.data();
	queueSubmitInfo.pWaitDstStageMask    = waitStageFlags.data();
	queueSubmitInfo.commandBufferCount   = (uint32_t)commandBuffers.size();
	queueSubmitInfo.pCommandBuffers      = commandBuffers.data();
	queueSubmitInfo.signalSemaphoreCount = 0;
	queueSubmitInfo.pSignalSemaphores    = nullptr;

	std::array submitInfos = {queueSubmitInfo};
	ThrowIfFailed(vkQueueSubmit(queue, (uint32_t)submitInfos.size(), submitInfos.data(), signalFence));
}

void Vulkan::DeviceQueues::QueueSubmit(VkQueue queue, std::span<VkCommandBuffer> commandBuffers, std::span<VkPipelineStageFlags> waitStageFlags, std::span<VkSemaphore> waitSemaphores, VkSemaphore signalSemaphore, VkFence signalFence) const
{
	assert(waitStageFlags.size() == waitSemaphores.size());
	std::array signalSemaphores = {signalSemaphore};

	//TODO: mGPU?
	//TODO: Performance query?
	VkSubmitInfo queueSubmitInfo;
	queueSubmitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	queueSubmitInfo.pNext                = nullptr;
	queueSubmitInfo.waitSemaphoreCount   = (uint32_t)waitSemaphores.size();
	queueSubmitInfo.pWaitSemaphores      = waitSemaphores.data();
	queueSubmitInfo.pWaitDstStageMask    = waitStageFlags.data();
	queueSubmitInfo.commandBufferCount   = (uint32_t)commandBuffers.size();
	queueSubmitInfo.pCommandBuffers      = commandBuffers.data();
	queueSubmitInfo.signalSemaphoreCount = (uint32_t)signalSemaphores.size();
	queueSubmitInfo.pSignalSemaphores    = signalSemaphores.data();

	std::array submitInfos = {queueSubmitInfo};
	ThrowIfFailed(vkQueueSubmit(queue, (uint32_t)submitInfos.size(), submitInfos.data(), signalFence));
}