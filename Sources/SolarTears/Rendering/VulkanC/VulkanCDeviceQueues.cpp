#include "VulkanCDeviceQueues.hpp"
#include <cassert>
#include <vector>
#include "VulkanCFunctions.hpp"
#include "VulkanCUtils.hpp"
#include <array>

VulkanCBindings::DeviceQueues::DeviceQueues(VkPhysicalDevice physicalDevice)
{
	mGraphicsQueue = VK_NULL_HANDLE;
	mComputeQueue  = VK_NULL_HANDLE;
	mTransferQueue = VK_NULL_HANDLE;

	FindDeviceQueueIndices(physicalDevice);
}

void VulkanCBindings::DeviceQueues::InitQueueHandles(VkDevice device)
{
	VkDeviceQueueInfo2 graphicsQueueInfo;
	graphicsQueueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
	graphicsQueueInfo.pNext            = nullptr;
	graphicsQueueInfo.flags            = 0; //I still don't use ANY queue priorities
	graphicsQueueInfo.queueIndex       = 0; //Use only one queue per family
	graphicsQueueInfo.queueFamilyIndex = mGraphicsQueueFamilyIndex;

	vkGetDeviceQueue2(device, &graphicsQueueInfo, &mGraphicsQueue);


	VkDeviceQueueInfo2 computeQueueInfo;
	computeQueueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
	computeQueueInfo.pNext            = nullptr;
	computeQueueInfo.flags            = 0; //I still don't use ANY queue priorities
	computeQueueInfo.queueIndex       = 0; //Use only one queue per family
	computeQueueInfo.queueFamilyIndex = mComputeQueueFamilyIndex;

	vkGetDeviceQueue2(device, &computeQueueInfo, &mComputeQueue);


	VkDeviceQueueInfo2 transferQueueInfo;
	transferQueueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
	transferQueueInfo.pNext            = nullptr;
	transferQueueInfo.flags            = 0; //I still don't use ANY queue priorities
	transferQueueInfo.queueIndex       = 0; //Use only one queue per family
	transferQueueInfo.queueFamilyIndex = mTransferQueueFamilyIndex;

	vkGetDeviceQueue2(device, &transferQueueInfo, &mTransferQueue);
}

uint32_t VulkanCBindings::DeviceQueues::GetGraphicsQueueFamilyIndex() const
{
	return mGraphicsQueueFamilyIndex;
}

uint32_t VulkanCBindings::DeviceQueues::GetComputeQueueFamilyIndex() const
{
	return mComputeQueueFamilyIndex;
}

uint32_t VulkanCBindings::DeviceQueues::GetTransferQueueFamilyIndex() const
{
	return mTransferQueueFamilyIndex;
}

void VulkanCBindings::DeviceQueues::GraphicsQueueWait() const
{
	ThrowIfFailed(vkQueueWaitIdle(mGraphicsQueue));
}

void VulkanCBindings::DeviceQueues::ComputeQueueWait() const
{
	ThrowIfFailed(vkQueueWaitIdle(mComputeQueue));
}

void VulkanCBindings::DeviceQueues::TransferQueueWait() const
{
	ThrowIfFailed(vkQueueWaitIdle(mTransferQueue));
}

void VulkanCBindings::DeviceQueues::GraphicsQueueSubmit(VkCommandBuffer commandBuffer)
{
	std::array commandBuffersToExecute = {commandBuffer};
	GraphicsQueueSubmit(commandBuffersToExecute.data(), (uint32_t)commandBuffersToExecute.size());
}

void VulkanCBindings::DeviceQueues::ComputeQueueSubmit(VkCommandBuffer commandBuffer)
{
	std::array commandBuffersToExecute = {commandBuffer};
	ComputeQueueSubmit(commandBuffersToExecute.data(), (uint32_t)commandBuffersToExecute.size());
}

void VulkanCBindings::DeviceQueues::TransferQueueSubmit(VkCommandBuffer commandBuffer)
{
	std::array commandBuffersToExecute = {commandBuffer};
	TransferQueueSubmit(commandBuffersToExecute.data(), (uint32_t)commandBuffersToExecute.size());
}

void VulkanCBindings::DeviceQueues::GraphicsQueueSubmit(VkCommandBuffer* commandBuffers, size_t commandBufferCount)
{
	QueueSubmit(mGraphicsQueue, commandBuffers, (uint32_t)commandBufferCount);
}

void VulkanCBindings::DeviceQueues::ComputeQueueSubmit(VkCommandBuffer* commandBuffers, size_t commandBufferCount)
{
	QueueSubmit(mComputeQueue, commandBuffers, (uint32_t)commandBufferCount);
}

void VulkanCBindings::DeviceQueues::TransferQueueSubmit(VkCommandBuffer* commandBuffers, size_t commandBufferCount)
{
	QueueSubmit(mTransferQueue, commandBuffers, (uint32_t)commandBufferCount);
}

void VulkanCBindings::DeviceQueues::GraphicsQueueSubmit(VkCommandBuffer* commandBuffers, size_t commandBufferCount, VkPipelineStageFlags waitStageFlags, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence signalFence)
{
	QueueSubmit(mGraphicsQueue, commandBuffers, (uint32_t)commandBufferCount, waitStageFlags, waitSemaphore, signalSemaphore, signalFence);
}

void VulkanCBindings::DeviceQueues::ComputeQueueSubmit(VkCommandBuffer* commandBuffers, size_t commandBufferCount, VkPipelineStageFlags waitStageFlags, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence signalFence)
{
	QueueSubmit(mComputeQueue, commandBuffers, (uint32_t)commandBufferCount, waitStageFlags, waitSemaphore, signalSemaphore, signalFence);
}

void VulkanCBindings::DeviceQueues::TransferQueueSubmit(VkCommandBuffer* commandBuffers, size_t commandBufferCount, VkPipelineStageFlags waitStageFlags, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence signalFence)
{
	QueueSubmit(mTransferQueue, commandBuffers, (uint32_t)commandBufferCount, waitStageFlags, waitSemaphore, signalSemaphore, signalFence);
}

void VulkanCBindings::DeviceQueues::FindDeviceQueueIndices(VkPhysicalDevice physicalDevice)
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

void VulkanCBindings::DeviceQueues::QueueSubmit(VkQueue queue, VkCommandBuffer* commandBuffers, uint32_t commandBufferCount)
{
	//TODO: mGPU?
	//TODO: Performance query?
	VkSubmitInfo queueSubmitInfo;
	queueSubmitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	queueSubmitInfo.pNext                = nullptr;
	queueSubmitInfo.waitSemaphoreCount   = 0;
	queueSubmitInfo.pWaitSemaphores      = nullptr;
	queueSubmitInfo.pWaitDstStageMask    = nullptr;
	queueSubmitInfo.commandBufferCount   = commandBufferCount;
	queueSubmitInfo.pCommandBuffers      = commandBuffers;
	queueSubmitInfo.signalSemaphoreCount = 0;
	queueSubmitInfo.pSignalSemaphores    = nullptr;

	std::array submitInfos = {queueSubmitInfo};
	ThrowIfFailed(vkQueueSubmit(queue, (uint32_t)submitInfos.size(), submitInfos.data(), nullptr));
}

void VulkanCBindings::DeviceQueues::QueueSubmit(VkQueue queue, VkCommandBuffer* commandBuffers, uint32_t commandBufferCount, VkPipelineStageFlags waitStageFlags, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence signalFence)
{
	std::array waitSemaphores   = {waitSemaphore};
	std::array waitStages       = {waitStageFlags};
	std::array signalSemaphores = {signalSemaphore};

	//TODO: mGPU?
	//TODO: Performance query?
	VkSubmitInfo queueSubmitInfo;
	queueSubmitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	queueSubmitInfo.pNext                = nullptr;
	queueSubmitInfo.waitSemaphoreCount   = (uint32_t)waitSemaphores.size();
	queueSubmitInfo.pWaitSemaphores      = waitSemaphores.data();
	queueSubmitInfo.pWaitDstStageMask    = waitStages.data();
	queueSubmitInfo.commandBufferCount   = commandBufferCount;
	queueSubmitInfo.pCommandBuffers      = commandBuffers;
	queueSubmitInfo.signalSemaphoreCount = (uint32_t)signalSemaphores.size();
	queueSubmitInfo.pSignalSemaphores    = signalSemaphores.data();

	std::array submitInfos = {queueSubmitInfo};
	ThrowIfFailed(vkQueueSubmit(queue, (uint32_t)submitInfos.size(), submitInfos.data(), signalFence));
}