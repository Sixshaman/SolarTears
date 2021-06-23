#include "VulkanWorkerCommandBuffers.hpp"
#include "VulkanUtils.hpp"
#include "VulkanFunctions.hpp"
#include "VulkanDeviceQueues.hpp"
#include "VulkanSwapChain.hpp"
#include "../../Core/ThreadPool.hpp"
#include "../Common/RenderingUtils.hpp"
#include <unordered_set>
#include <array>

Vulkan::WorkerCommandBuffers::WorkerCommandBuffers(VkDevice device, uint32_t workerThreadCount, const DeviceQueues* deviceQueues): mDeviceRef(device), mWorkerThreadCount(workerThreadCount)
{
	mCommandBufferIndexForGraphics = (uint32_t)(-1);
	mCommandBufferIndexForCompute  = (uint32_t)(-1);
	mCommandBufferIndexForTransfer = (uint32_t)(-1);

	InitCommandBuffers(deviceQueues->GetGraphicsQueueFamilyIndex(), deviceQueues->GetComputeQueueFamilyIndex(), deviceQueues->GetTransferQueueFamilyIndex());
}

Vulkan::WorkerCommandBuffers::~WorkerCommandBuffers()
{
	vkDeviceWaitIdle(mDeviceRef);

	for(size_t i = 0; i < mCommandPools.size(); i++)
	{
		SafeDestroyObject(vkDestroyCommandPool, mDeviceRef, mCommandPools[i]);
	}
}

void Vulkan::WorkerCommandBuffers::CreatePresentCommandBuffers(const SwapChain* swapChain)
{
	InitPresentQueueCommandBuffers(swapChain->GetPresentQueueFamilyIndex());
}

uint32_t Vulkan::WorkerCommandBuffers::GetWorkerThreadCount() const
{
	return mWorkerThreadCount;
}

void Vulkan::WorkerCommandBuffers::InitCommandBuffers(uint32_t graphicsQueueFamilyIndex, uint32_t computeQueueFamilyIndex, uint32_t transferQueueFamilyIndex)
{
	//Always initialize different buffers, even if hardware doesn't have separate compute/graphics queues.
	//That's to have the same logic in frame graphs with different queue types
	std::unordered_set<uint32_t> queues = {graphicsQueueFamilyIndex, computeQueueFamilyIndex, transferQueueFamilyIndex};
	mSeparateQueueCount                 = (uint32_t)queues.size();

	//1 for each worker thread plus 1 for the main thread per queue per frame
	for(uint32_t t = 0; t < mWorkerThreadCount + 1; t++)
	{
		for(uint32_t f = 0; f < Utils::InFlightFrameCount; f++)
		{
			for(uint32_t queueFamilyIndex: queues)
			{
				VkCommandPool commandPool = CreateCommandPool(queueFamilyIndex);
				mCommandPools.push_back(commandPool);

				VkCommandBuffer commandBuffer = CreateCommandBuffer(commandPool);
				mCommandBuffers.push_back(commandBuffer);
			}
		}
	}

	for(uint32_t f = 0; f < Utils::InFlightFrameCount; f++)
	{
		//Add present command buffers to the end
		mCommandPools.push_back(VK_NULL_HANDLE);
		mCommandBuffers.push_back(VK_NULL_HANDLE);
	}

	for(uint32_t f = 0; f < Utils::InFlightFrameCount; f++)
	{
		//Add present command buffers to the end
		mCommandPools.push_back(VK_NULL_HANDLE);
		mCommandBuffers.push_back(VK_NULL_HANDLE);
	}

	mCommandBufferIndexForGraphics = (uint32_t)std::distance(queues.begin(), queues.find(graphicsQueueFamilyIndex));
	mCommandBufferIndexForCompute  = (uint32_t)std::distance(queues.begin(), queues.find(computeQueueFamilyIndex));
	mCommandBufferIndexForTransfer = (uint32_t)std::distance(queues.begin(), queues.find(transferQueueFamilyIndex));
}

void Vulkan::WorkerCommandBuffers::InitPresentQueueCommandBuffers(uint32_t presentQueueFamilyIndex)
{
	//Buffers for acquire pass
	for(uint32_t f = 0; f < Utils::InFlightFrameCount; f++)
	{
		size_t acquireBufferIndex = (size_t)(mWorkerThreadCount + 1) * (size_t)Utils::InFlightFrameCount * mSeparateQueueCount + f;
		SafeDestroyObject(vkDestroyCommandPool, mDeviceRef, mCommandPools[acquireBufferIndex]);

		//Add present command buffers to the end
		VkCommandPool commandPool = CreateCommandPool(presentQueueFamilyIndex);
		mCommandPools[acquireBufferIndex] = commandPool;

		VkCommandBuffer commandBuffer = CreateCommandBuffer(commandPool);
		mCommandBuffers[acquireBufferIndex] = commandBuffer;
	}

	//Buffers for present pass
	for(uint32_t f = 0; f < Utils::InFlightFrameCount; f++)
	{
		size_t presentBufferIndex = (size_t)(mWorkerThreadCount + 1) * (size_t)Utils::InFlightFrameCount * mSeparateQueueCount + (size_t)Utils::InFlightFrameCount + f;
		SafeDestroyObject(vkDestroyCommandPool, mDeviceRef, mCommandPools[presentBufferIndex]);

		//Add present command buffers to the end
		VkCommandPool commandPool = CreateCommandPool(presentQueueFamilyIndex);
		mCommandPools[presentBufferIndex] = commandPool;

		VkCommandBuffer commandBuffer = CreateCommandBuffer(commandPool);
		mCommandBuffers[presentBufferIndex] = commandBuffer;
	}
}

VkCommandPool Vulkan::WorkerCommandBuffers::CreateCommandPool(uint32_t queueFamilyIndex)
{
	//Do not reset individual buffers
	VkCommandPoolCreateInfo commandPoolCreateInfo;
	commandPoolCreateInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.pNext            = nullptr;
	commandPoolCreateInfo.flags            = 0;
	commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;

	VkCommandPool commandPool = nullptr;
	ThrowIfFailed(vkCreateCommandPool(mDeviceRef, &commandPoolCreateInfo, nullptr, &commandPool));

	return commandPool;
}

VkCommandBuffer Vulkan::WorkerCommandBuffers::CreateCommandBuffer(VkCommandPool commandPool)
{
	std::array<VkCommandBuffer, 1> allocatedCommandBuffers;

	//TODO: secondary command buffers
	VkCommandBufferAllocateInfo commandBufferAllocateInfo;
	commandBufferAllocateInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.pNext              = nullptr;
	commandBufferAllocateInfo.commandPool        = commandPool;
	commandBufferAllocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = (uint32_t)allocatedCommandBuffers.size();

	ThrowIfFailed(vkAllocateCommandBuffers(mDeviceRef, &commandBufferAllocateInfo, allocatedCommandBuffers.data()));
	return allocatedCommandBuffers[0];
}

VkCommandPool Vulkan::WorkerCommandBuffers::GetMainThreadGraphicsCommandPool(uint32_t frameIndex) const
{
	//Main thread command buffers/pools are always at the end
	size_t poolIndex = (size_t)mWorkerThreadCount * (size_t)Utils::InFlightFrameCount * mSeparateQueueCount + frameIndex * mSeparateQueueCount + mCommandBufferIndexForGraphics;
	return mCommandPools[poolIndex];
}

VkCommandPool Vulkan::WorkerCommandBuffers::GetMainThreadComputeCommandPool(uint32_t frameIndex) const
{
	//Main thread command buffers/pools are always at the end
	size_t poolIndex = (size_t)mWorkerThreadCount * (size_t)Utils::InFlightFrameCount * mSeparateQueueCount + frameIndex * mSeparateQueueCount + mCommandBufferIndexForCompute;
	return mCommandPools[poolIndex];
}

VkCommandPool Vulkan::WorkerCommandBuffers::GetMainThreadTransferCommandPool(uint32_t frameIndex) const
{
	//Main thread command buffers/pools are always at the end
	size_t poolIndex = (size_t)mWorkerThreadCount * (size_t)Utils::InFlightFrameCount * mSeparateQueueCount + frameIndex * mSeparateQueueCount + mCommandBufferIndexForTransfer;
	return mCommandPools[poolIndex];
}

VkCommandBuffer Vulkan::WorkerCommandBuffers::GetMainThreadGraphicsCommandBuffer(uint32_t frameIndex) const
{
	//Main thread command buffers/pools are always at the end
	size_t bufferIndex = (size_t)mWorkerThreadCount * (size_t)Utils::InFlightFrameCount * mSeparateQueueCount + frameIndex * mSeparateQueueCount + mCommandBufferIndexForGraphics;
	return mCommandBuffers[bufferIndex];
}

VkCommandBuffer Vulkan::WorkerCommandBuffers::GetMainThreadComputeCommandBuffer(uint32_t frameIndex) const
{
	//Main thread command buffers/pools are always at the end
	size_t bufferIndex = (size_t)mWorkerThreadCount * (size_t)Utils::InFlightFrameCount * mSeparateQueueCount + frameIndex * mSeparateQueueCount + mCommandBufferIndexForCompute;
	return mCommandBuffers[bufferIndex];
}

VkCommandBuffer Vulkan::WorkerCommandBuffers::GetMainThreadTransferCommandBuffer(uint32_t frameIndex) const
{
	//Main thread command buffers/pools are always at the end
	size_t bufferIndex = (size_t)mWorkerThreadCount * (size_t)Utils::InFlightFrameCount * mSeparateQueueCount + frameIndex * mSeparateQueueCount + mCommandBufferIndexForTransfer;
	return mCommandBuffers[bufferIndex];
}

VkCommandPool Vulkan::WorkerCommandBuffers::GetThreadGraphicsCommandPool(uint32_t threadIndex, uint32_t frameIndex) const
{
	size_t poolIndex = (size_t)threadIndex * (size_t)Utils::InFlightFrameCount * mSeparateQueueCount + frameIndex * mSeparateQueueCount + mCommandBufferIndexForGraphics;
	return mCommandPools[poolIndex];
}

VkCommandPool Vulkan::WorkerCommandBuffers::GetThreadComputeCommandPool(uint32_t threadIndex, uint32_t frameIndex) const
{
	size_t poolIndex = (size_t)threadIndex * (size_t)Utils::InFlightFrameCount * mSeparateQueueCount + frameIndex * mSeparateQueueCount + mCommandBufferIndexForCompute;
	return mCommandPools[poolIndex];
}

VkCommandPool Vulkan::WorkerCommandBuffers::GetThreadTransferCommandPool(uint32_t threadIndex, uint32_t frameIndex) const
{
	size_t poolIndex = (size_t)threadIndex * (size_t)Utils::InFlightFrameCount * mSeparateQueueCount + frameIndex * mSeparateQueueCount + mCommandBufferIndexForTransfer;
	return mCommandPools[poolIndex];
}

VkCommandBuffer Vulkan::WorkerCommandBuffers::GetThreadGraphicsCommandBuffer(uint32_t threadIndex, uint32_t frameIndex) const
{
	size_t bufferIndex = (size_t)threadIndex * (size_t)Utils::InFlightFrameCount * mSeparateQueueCount + frameIndex * mSeparateQueueCount + mCommandBufferIndexForGraphics;
	return mCommandBuffers[bufferIndex];
}

VkCommandBuffer Vulkan::WorkerCommandBuffers::GetThreadComputeCommandBuffer(uint32_t threadIndex, uint32_t frameIndex) const
{
	size_t bufferIndex = (size_t)threadIndex * (size_t)Utils::InFlightFrameCount * mSeparateQueueCount + frameIndex * mSeparateQueueCount + mCommandBufferIndexForCompute;
	return mCommandBuffers[bufferIndex];
}

VkCommandBuffer Vulkan::WorkerCommandBuffers::GetThreadTransferCommandBuffer(uint32_t threadIndex, uint32_t frameIndex) const
{
	size_t bufferIndex = (size_t)threadIndex * (size_t)Utils::InFlightFrameCount * mSeparateQueueCount + frameIndex * mSeparateQueueCount + mCommandBufferIndexForTransfer;
	return mCommandBuffers[bufferIndex];
}

VkCommandPool Vulkan::WorkerCommandBuffers::GetMainThreadAcquireCommandPool(uint32_t frameIndex) const
{
	size_t bufferIndex = (size_t)(mWorkerThreadCount + 1) * (size_t)Utils::InFlightFrameCount * mSeparateQueueCount + frameIndex;
	return mCommandPools[bufferIndex];
}

VkCommandPool Vulkan::WorkerCommandBuffers::GetMainThreadPresentCommandPool(uint32_t frameIndex) const
{
	size_t bufferIndex = (size_t)(mWorkerThreadCount + 1) * (size_t)Utils::InFlightFrameCount * mSeparateQueueCount + (size_t)Utils::InFlightFrameCount + frameIndex;
	return mCommandPools[bufferIndex];
}

VkCommandBuffer Vulkan::WorkerCommandBuffers::GetMainThreadAcquireCommandBuffer(uint32_t frameIndex) const
{
	size_t bufferIndex = (size_t)(mWorkerThreadCount + 1) * (size_t)Utils::InFlightFrameCount * mSeparateQueueCount + frameIndex;
	return mCommandBuffers[bufferIndex];
}

VkCommandBuffer Vulkan::WorkerCommandBuffers::GetMainThreadPresentCommandBuffer(uint32_t frameIndex) const
{
	size_t bufferIndex = (size_t)(mWorkerThreadCount + 1) * (size_t)Utils::InFlightFrameCount * mSeparateQueueCount + (size_t)Utils::InFlightFrameCount + frameIndex;
	return mCommandBuffers[bufferIndex];
}
