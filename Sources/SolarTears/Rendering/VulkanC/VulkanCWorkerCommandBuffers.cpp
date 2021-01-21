#include "VulkanCWorkerCommandBuffers.hpp"
#include "VulkanCUtils.hpp"
#include "VulkanCFunctions.hpp"
#include "VulkanCDeviceQueues.hpp"
#include "../../Core/ThreadPool.hpp"
#include <unordered_set>
#include <array>

VulkanCBindings::WorkerCommandBuffers::WorkerCommandBuffers(VkDevice device, uint32_t workerThreadCount, const DeviceQueues* deviceQueues): mDeviceRef(device), mWorkerThreadCount(workerThreadCount),
	                                                                                                                                        mCommandBufferIndexForGraphics((uint32_t)(-1)), 
	                                                                                                                                        mCommandBufferIndexForCompute((uint32_t)(-1)), 
	                                                                                                                                        mCommandBufferIndexForTransfer((uint32_t)(-1))
{
	InitCommandBuffers(deviceQueues->GetGraphicsQueueFamilyIndex(), deviceQueues->GetComputeQueueFamilyIndex(), deviceQueues->GetTransferQueueFamilyIndex());
}

VulkanCBindings::WorkerCommandBuffers::~WorkerCommandBuffers()
{
	vkDeviceWaitIdle(mDeviceRef);

	for(size_t i = 0; i < mCommandPools.size(); i++)
	{
		SafeDestroyObject(vkDestroyCommandPool, mDeviceRef, mCommandPools[i]);
	}
}

uint32_t VulkanCBindings::WorkerCommandBuffers::GetWorkerThreadCount() const
{
	return mWorkerThreadCount;
}

void VulkanCBindings::WorkerCommandBuffers::InitCommandBuffers(uint32_t graphicsQueueFamilyIndex, uint32_t computeQueueFamilyIndex, uint32_t transferQueueFamilyIndex)
{
	std::unordered_set<uint32_t> queues = {graphicsQueueFamilyIndex, computeQueueFamilyIndex, transferQueueFamilyIndex};
	mSeparateQueueCount                 = (uint32_t)queues.size();

	//1 for each worker thread plus 1 for the main thread per queue per frame
	for(uint32_t t = 0; t < mWorkerThreadCount + 1; t++)
	{
		for(uint32_t f = 0; f < VulkanCBindings::VulkanUtils::InFlightFrameCount; f++)
		{
			for(uint32_t queueIndex: queues)
			{
				std::array<VkCommandBuffer, 1> allocatedCommandBuffers;

				//Do not reset individual buffers
				VkCommandPoolCreateInfo commandPoolCreateInfo;
				commandPoolCreateInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
				commandPoolCreateInfo.pNext            = nullptr;
				commandPoolCreateInfo.flags            = 0;
				commandPoolCreateInfo.queueFamilyIndex = queueIndex;

				VkCommandPool commandPool = nullptr;
				ThrowIfFailed(vkCreateCommandPool(mDeviceRef, &commandPoolCreateInfo, nullptr, &commandPool));
				mCommandPools.push_back(commandPool);

				//TODO: secondary command buffers
				VkCommandBufferAllocateInfo commandBufferAllocateInfo;
				commandBufferAllocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
				commandBufferAllocateInfo.pNext              = nullptr;
				commandBufferAllocateInfo.commandPool        = mCommandPools.back();
				commandBufferAllocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
				commandBufferAllocateInfo.commandBufferCount = (uint32_t)allocatedCommandBuffers.size();

				ThrowIfFailed(vkAllocateCommandBuffers(mDeviceRef, &commandBufferAllocateInfo, allocatedCommandBuffers.data()));
				mCommandBuffers.push_back(allocatedCommandBuffers[0]);
			}
		}
	}

	mCommandBufferIndexForGraphics = (uint32_t)std::distance(queues.begin(), queues.find(graphicsQueueFamilyIndex));
	mCommandBufferIndexForCompute  = (uint32_t)std::distance(queues.begin(), queues.find(computeQueueFamilyIndex));
	mCommandBufferIndexForTransfer = (uint32_t)std::distance(queues.begin(), queues.find(transferQueueFamilyIndex));
}

VkCommandPool VulkanCBindings::WorkerCommandBuffers::GetMainThreadGraphicsCommandPool(uint32_t frameIndex) const
{
	//Main thread command buffers/pools are always at the end
	return mCommandPools[(size_t)mWorkerThreadCount * frameIndex * mSeparateQueueCount + mCommandBufferIndexForGraphics];
}

VkCommandPool VulkanCBindings::WorkerCommandBuffers::GetMainThreadComputeCommandPool(uint32_t frameIndex) const
{
	//Main thread command buffers/pools are always at the end
	return mCommandPools[(size_t)mWorkerThreadCount * frameIndex * mSeparateQueueCount + mCommandBufferIndexForCompute];
}

VkCommandPool VulkanCBindings::WorkerCommandBuffers::GetMainThreadTransferCommandPool(uint32_t frameIndex) const
{
	//Main thread command buffers/pools are always at the end
	return mCommandPools[(size_t)mWorkerThreadCount * frameIndex * mSeparateQueueCount + mCommandBufferIndexForTransfer];
}

VkCommandBuffer VulkanCBindings::WorkerCommandBuffers::GetMainThreadGraphicsCommandBuffer(uint32_t frameIndex) const
{
	//Main thread command buffers/pools are always at the end
	return mCommandBuffers[(size_t)mWorkerThreadCount * frameIndex * mSeparateQueueCount + mCommandBufferIndexForGraphics];
}

VkCommandBuffer VulkanCBindings::WorkerCommandBuffers::GetMainThreadComputeCommandBuffer(uint32_t frameIndex) const
{
	//Main thread command buffers/pools are always at the end
	return mCommandBuffers[(size_t)mWorkerThreadCount * frameIndex * mSeparateQueueCount + mCommandBufferIndexForCompute];
}

VkCommandBuffer VulkanCBindings::WorkerCommandBuffers::GetMainThreadTransferCommandBuffer(uint32_t frameIndex) const
{
	//Main thread command buffers/pools are always at the end
	return mCommandBuffers[(size_t)mWorkerThreadCount * frameIndex * mSeparateQueueCount + mCommandBufferIndexForTransfer];
}

VkCommandPool VulkanCBindings::WorkerCommandBuffers::GetThreadGraphicsCommandPool(uint32_t threadIndex, uint32_t frameIndex) const
{
	return mCommandPools[(size_t)threadIndex * frameIndex * mSeparateQueueCount + mCommandBufferIndexForGraphics];
}

VkCommandPool VulkanCBindings::WorkerCommandBuffers::GetThreadComputeCommandPool(uint32_t threadIndex, uint32_t frameIndex) const
{
	return mCommandPools[(size_t)threadIndex * frameIndex * mSeparateQueueCount + mCommandBufferIndexForCompute];
}

VkCommandPool VulkanCBindings::WorkerCommandBuffers::GetThreadTransferCommandPool(uint32_t threadIndex, uint32_t frameIndex) const
{
	return mCommandPools[(size_t)threadIndex * frameIndex * mSeparateQueueCount + mCommandBufferIndexForTransfer];
}

VkCommandBuffer VulkanCBindings::WorkerCommandBuffers::GetThreadGraphicsCommandBuffer(uint32_t threadIndex, uint32_t frameIndex) const
{
	return mCommandBuffers[(size_t)threadIndex * frameIndex * mSeparateQueueCount + mCommandBufferIndexForGraphics];
}

VkCommandBuffer VulkanCBindings::WorkerCommandBuffers::GetThreadComputeCommandBuffer(uint32_t threadIndex, uint32_t frameIndex) const
{
	return mCommandBuffers[(size_t)threadIndex * frameIndex * mSeparateQueueCount + mCommandBufferIndexForCompute];
}

VkCommandBuffer VulkanCBindings::WorkerCommandBuffers::GetThreadTransferCommandBuffer(uint32_t threadIndex, uint32_t frameIndex) const
{
	return mCommandBuffers[(size_t)threadIndex * frameIndex * mSeparateQueueCount + mCommandBufferIndexForTransfer];
}