#include "D3D12DeviceQueues.hpp"
#include <cassert>
#include <vector>
#include "D3D12Utils.hpp"
#include <array>

D3D12::DeviceQueues::DeviceQueues(ID3D12Device* device)
{
	CreateQueueObjects(device);

	mGraphicsFenceValue = 0;
	mComputeFenceValue  = 0;
	mCopyFenceValue     = 0;

	THROW_IF_FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(mGraphicsFence.put())));
	THROW_IF_FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(mComputeFence.put())));
	THROW_IF_FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(mCopyFence.put())));

	mGraphicsFenceCompletionEvent = wil::unique_event(CreateEventEx(nullptr, L"Graphics fence wait", 0, EVENT_ALL_ACCESS));
	mComputeFenceCompletionEvent  = wil::unique_event(CreateEventEx(nullptr, L"Compute fence wait",  0, EVENT_ALL_ACCESS));
	mCopyFenceCompletionEvent     = wil::unique_event(CreateEventEx(nullptr, L"Copy fence wait",     0, EVENT_ALL_ACCESS));
}

ID3D12CommandQueue* D3D12::DeviceQueues::GraphicsQueueHandle() const
{
	return mGraphicsQueue.get();
}

ID3D12CommandQueue* D3D12::DeviceQueues::ComputeQueueHandle() const
{
	return mComputeQueue.get();
}

ID3D12CommandQueue* D3D12::DeviceQueues::CopyQueueHandle() const
{
	return mCopyQueue.get();
}

void D3D12::DeviceQueues::GraphicsQueueWait()
{
	THROW_IF_FAILED(mGraphicsQueue->Signal(mGraphicsFence.get(), ++mGraphicsFenceValue));

	THROW_IF_FAILED(mGraphicsFence->SetEventOnCompletion(mGraphicsFenceValue, mGraphicsFenceCompletionEvent.get()));
	WaitForSingleObject(mGraphicsFenceCompletionEvent.get(), INFINITE);
}

void D3D12::DeviceQueues::ComputeQueueWait()
{
	THROW_IF_FAILED(mComputeQueue->Signal(mComputeFence.get(), ++mComputeFenceValue));

	THROW_IF_FAILED(mComputeFence->SetEventOnCompletion(mComputeFenceValue, mComputeFenceCompletionEvent.get()));
	WaitForSingleObject(mComputeFenceCompletionEvent.get(), INFINITE);
}

void D3D12::DeviceQueues::CopyQueueWait()
{
	THROW_IF_FAILED(mCopyQueue->Signal(mCopyFence.get(), ++mCopyFenceValue));

	THROW_IF_FAILED(mCopyFence->SetEventOnCompletion(mCopyFenceValue, mCopyFenceCompletionEvent.get()));
	WaitForSingleObject(mCopyFenceCompletionEvent.get(), INFINITE);
}

void D3D12::DeviceQueues::AllQueuesWait()
{
	THROW_IF_FAILED(mGraphicsQueue->Signal(mGraphicsFence.get(), ++mGraphicsFenceValue));
	THROW_IF_FAILED(mComputeQueue->Signal(mComputeFence.get(),   ++mComputeFenceValue));
	THROW_IF_FAILED(mCopyQueue->Signal(mCopyFence.get(),         ++mCopyFenceValue));

	THROW_IF_FAILED(mGraphicsFence->SetEventOnCompletion(mGraphicsFenceValue, mGraphicsFenceCompletionEvent.get()));
	THROW_IF_FAILED(mComputeFence->SetEventOnCompletion(mComputeFenceValue,   mComputeFenceCompletionEvent.get()));
	THROW_IF_FAILED(mCopyFence->SetEventOnCompletion(mCopyFenceValue,         mCopyFenceCompletionEvent.get()));

	std::array fenceEvents = {mGraphicsFenceCompletionEvent.get(), mComputeFenceCompletionEvent.get(), mCopyFenceCompletionEvent.get()};
	WaitForMultipleObjects((DWORD)fenceEvents.size(), fenceEvents.data(), TRUE, INFINITE);
}

//void VulkanCBindings::DeviceQueues::InitQueueHandles(VkDevice device)
//{
//	VkDeviceQueueInfo2 graphicsQueueInfo;
//	graphicsQueueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
//	graphicsQueueInfo.pNext            = nullptr;
//	graphicsQueueInfo.flags            = 0; //I still don't use ANY queue priorities
//	graphicsQueueInfo.queueIndex       = 0; //Use only one queue per family
//	graphicsQueueInfo.queueFamilyIndex = mGraphicsQueueFamilyIndex;
//
//	vkGetDeviceQueue2(device, &graphicsQueueInfo, &mGraphicsQueue);
//	if(mGraphicsQueue == VK_NULL_HANDLE) //Some older AMD cards return NULL on vkGetDeviceQueue2
//	{
//		vkGetDeviceQueue(device, mGraphicsQueueFamilyIndex, 0, &mGraphicsQueue);
//	}
//
//	VkDeviceQueueInfo2 computeQueueInfo;
//	computeQueueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
//	computeQueueInfo.pNext            = nullptr;
//	computeQueueInfo.flags            = 0; //I still don't use ANY queue priorities
//	computeQueueInfo.queueIndex       = 0; //Use only one queue per family
//	computeQueueInfo.queueFamilyIndex = mComputeQueueFamilyIndex;
//
//	vkGetDeviceQueue2(device, &computeQueueInfo, &mComputeQueue);
//	if(mComputeQueue == VK_NULL_HANDLE) //Some older AMD cards return NULL on vkGetDeviceQueue2
//	{
//		vkGetDeviceQueue(device, mComputeQueueFamilyIndex, 0, &mComputeQueue);
//	}
//
//
//	VkDeviceQueueInfo2 transferQueueInfo;
//	transferQueueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
//	transferQueueInfo.pNext            = nullptr;
//	transferQueueInfo.flags            = 0; //I still don't use ANY queue priorities
//	transferQueueInfo.queueIndex       = 0; //Use only one queue per family
//	transferQueueInfo.queueFamilyIndex = mTransferQueueFamilyIndex;
//
//	vkGetDeviceQueue2(device, &transferQueueInfo, &mTransferQueue);
//	if(mTransferQueue == VK_NULL_HANDLE) //Some older AMD cards return NULL on vkGetDeviceQueue2
//	{
//		vkGetDeviceQueue(device, mTransferQueueFamilyIndex, 0, &mTransferQueue);
//	}
//}

//void VulkanCBindings::DeviceQueues::GraphicsQueueSubmit(VkCommandBuffer commandBuffer) const
//{
//	std::array commandBuffersToExecute = {commandBuffer};
//	GraphicsQueueSubmit(commandBuffersToExecute.data(), (uint32_t)commandBuffersToExecute.size());
//}
//
//void VulkanCBindings::DeviceQueues::ComputeQueueSubmit(VkCommandBuffer commandBuffer) const
//{
//	std::array commandBuffersToExecute = {commandBuffer};
//	ComputeQueueSubmit(commandBuffersToExecute.data(), (uint32_t)commandBuffersToExecute.size());
//}
//
//void VulkanCBindings::DeviceQueues::TransferQueueSubmit(VkCommandBuffer commandBuffer) const
//{
//	std::array commandBuffersToExecute = {commandBuffer};
//	TransferQueueSubmit(commandBuffersToExecute.data(), (uint32_t)commandBuffersToExecute.size());
//}
//
//void VulkanCBindings::DeviceQueues::GraphicsQueueSubmit(VkCommandBuffer commandBuffer, VkSemaphore signalSemaphore) const
//{
//	std::array commandBuffersToExecute = {commandBuffer};
//	QueueSubmit(mGraphicsQueue, commandBuffersToExecute.data(), (uint32_t)commandBuffersToExecute.size(), signalSemaphore, nullptr);
//}
//
//void VulkanCBindings::DeviceQueues::ComputeQueueSubmit(VkCommandBuffer commandBuffer, VkSemaphore signalSemaphore) const
//{
//	std::array commandBuffersToExecute = {commandBuffer};
//	QueueSubmit(mComputeQueue, commandBuffersToExecute.data(), (uint32_t)commandBuffersToExecute.size(), signalSemaphore, nullptr);
//}
//
//void VulkanCBindings::DeviceQueues::TransferQueueSubmit(VkCommandBuffer commandBuffer, VkSemaphore signalSemaphore) const
//{
//	std::array commandBuffersToExecute = {commandBuffer};
//	QueueSubmit(mTransferQueue, commandBuffersToExecute.data(), (uint32_t)commandBuffersToExecute.size(), signalSemaphore, nullptr);
//}
//
//void VulkanCBindings::DeviceQueues::GraphicsQueueSubmit(VkCommandBuffer commandBuffer, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStageFlags) const
//{
//	std::array commandBuffersToExecute = {commandBuffer};
//	QueueSubmit(mGraphicsQueue, commandBuffersToExecute.data(), (uint32_t)commandBuffersToExecute.size(), waitStageFlags, waitSemaphore, nullptr);
//}
//
//void VulkanCBindings::DeviceQueues::ComputeQueueSubmit(VkCommandBuffer commandBuffer, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStageFlags) const
//{
//	std::array commandBuffersToExecute = {commandBuffer};
//	QueueSubmit(mComputeQueue, commandBuffersToExecute.data(), (uint32_t)commandBuffersToExecute.size(), waitStageFlags, waitSemaphore, nullptr);
//}
//
//void VulkanCBindings::DeviceQueues::TransferQueueSubmit(VkCommandBuffer commandBuffer, VkSemaphore waitSemaphore, VkPipelineStageFlags waitStageFlags) const
//{
//	std::array commandBuffersToExecute = {commandBuffer};
//	QueueSubmit(mTransferQueue, commandBuffersToExecute.data(), (uint32_t)commandBuffersToExecute.size(), waitStageFlags, waitSemaphore, nullptr);
//}
//
//void VulkanCBindings::DeviceQueues::GraphicsQueueSubmit(VkCommandBuffer* commandBuffers, size_t commandBufferCount) const
//{
//	QueueSubmit(mGraphicsQueue, commandBuffers, (uint32_t)commandBufferCount);
//}
//
//void VulkanCBindings::DeviceQueues::ComputeQueueSubmit(VkCommandBuffer* commandBuffers, size_t commandBufferCount) const
//{
//	QueueSubmit(mComputeQueue, commandBuffers, (uint32_t)commandBufferCount);
//}
//
//void VulkanCBindings::DeviceQueues::TransferQueueSubmit(VkCommandBuffer* commandBuffers, size_t commandBufferCount) const
//{
//	QueueSubmit(mTransferQueue, commandBuffers, (uint32_t)commandBufferCount);
//}
//
//void VulkanCBindings::DeviceQueues::GraphicsQueueSubmit(VkCommandBuffer* commandBuffers, size_t commandBufferCount, VkPipelineStageFlags waitStageFlags, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence signalFence) const
//{
//	QueueSubmit(mGraphicsQueue, commandBuffers, (uint32_t)commandBufferCount, waitStageFlags, waitSemaphore, signalSemaphore, signalFence);
//}
//
//void VulkanCBindings::DeviceQueues::ComputeQueueSubmit(VkCommandBuffer* commandBuffers, size_t commandBufferCount, VkPipelineStageFlags waitStageFlags, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence signalFence) const
//{
//	QueueSubmit(mComputeQueue, commandBuffers, (uint32_t)commandBufferCount, waitStageFlags, waitSemaphore, signalSemaphore, signalFence);
//}
//
//void VulkanCBindings::DeviceQueues::TransferQueueSubmit(VkCommandBuffer* commandBuffers, size_t commandBufferCount, VkPipelineStageFlags waitStageFlags, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence signalFence) const
//{
//	QueueSubmit(mTransferQueue, commandBuffers, (uint32_t)commandBufferCount, waitStageFlags, waitSemaphore, signalSemaphore, signalFence);
//}
//
//void VulkanCBindings::DeviceQueues::FindDeviceQueueIndices(VkPhysicalDevice physicalDevice)
//{
//	//Use dedicated queues
//	uint32_t graphicsQueueFamilyIndex = (uint32_t)(-1);
//	uint32_t computeQueueFamilyIndex  = (uint32_t)(-1);
//	uint32_t transferQueueFamilyIndex = (uint32_t)(-1);
//
//	std::vector<VkQueueFamilyProperties> queueFamiliesProperties;
//
//	uint32_t queueFamilyCount = 0;
//	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
//
//	queueFamiliesProperties.resize(queueFamilyCount);
//	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamiliesProperties.data());
//	for(uint32_t queueFamilyIndex = 0; queueFamilyIndex < (uint32_t)queueFamiliesProperties.size(); queueFamilyIndex++)
//	{
//		//First found graphics queue
//		if((graphicsQueueFamilyIndex == (uint32_t)(-1)) && ((queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0))
//		{
//			graphicsQueueFamilyIndex = queueFamilyIndex;
//		}
//		
//		//First found dedicated compute queue
//		if((computeQueueFamilyIndex == (uint32_t)(-1)) && ((queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) && ((queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
//		{
//			computeQueueFamilyIndex = queueFamilyIndex;
//		}
//
//		//First found dedicated transfer queue
//		if((transferQueueFamilyIndex == (uint32_t)(-1)) && ((queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_TRANSFER_BIT) != 0) && ((queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueFamiliesProperties[queueFamilyIndex].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
//		{
//			transferQueueFamilyIndex = queueFamilyIndex;
//		}
//	}
//
//	//Some of the queues are not found, use non-dedicated ones instead
//	if(computeQueueFamilyIndex == (uint32_t)(-1))
//	{
//		computeQueueFamilyIndex = graphicsQueueFamilyIndex;
//	}
//
//	if(transferQueueFamilyIndex == (uint32_t)(-1))
//	{
//		transferQueueFamilyIndex = computeQueueFamilyIndex;
//	}
//
//	assert(graphicsQueueFamilyIndex != (uint32_t)(-1));
//	assert(computeQueueFamilyIndex  != (uint32_t)(-1));
//	assert(transferQueueFamilyIndex != (uint32_t)(-1));
//
//	mGraphicsQueueFamilyIndex = graphicsQueueFamilyIndex;
//	mComputeQueueFamilyIndex  = computeQueueFamilyIndex;
//	mTransferQueueFamilyIndex = transferQueueFamilyIndex;
//}
//
//void VulkanCBindings::DeviceQueues::QueueSubmit(VkQueue queue, VkCommandBuffer* commandBuffers, uint32_t commandBufferCount) const
//{
//	//TODO: mGPU?
//	//TODO: Performance query?
//	VkSubmitInfo queueSubmitInfo;
//	queueSubmitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
//	queueSubmitInfo.pNext                = nullptr;
//	queueSubmitInfo.waitSemaphoreCount   = 0;
//	queueSubmitInfo.pWaitSemaphores      = nullptr;
//	queueSubmitInfo.pWaitDstStageMask    = nullptr;
//	queueSubmitInfo.commandBufferCount   = commandBufferCount;
//	queueSubmitInfo.pCommandBuffers      = commandBuffers;
//	queueSubmitInfo.signalSemaphoreCount = 0;
//	queueSubmitInfo.pSignalSemaphores    = nullptr;
//
//	std::array submitInfos = {queueSubmitInfo};
//	ThrowIfFailed(vkQueueSubmit(queue, (uint32_t)submitInfos.size(), submitInfos.data(), nullptr));
//}
//
//void VulkanCBindings::DeviceQueues::QueueSubmit(VkQueue queue, VkCommandBuffer* commandBuffers, uint32_t commandBufferCount, VkSemaphore signalSemaphore, VkFence signalFence) const
//{
//	std::array signalSemaphores = {signalSemaphore};
//
//	//TODO: mGPU?
//	//TODO: Performance query?
//	VkSubmitInfo queueSubmitInfo;
//	queueSubmitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
//	queueSubmitInfo.pNext                = nullptr;
//	queueSubmitInfo.waitSemaphoreCount   = 0;
//	queueSubmitInfo.pWaitSemaphores      = nullptr;
//	queueSubmitInfo.pWaitDstStageMask    = nullptr;
//	queueSubmitInfo.commandBufferCount   = commandBufferCount;
//	queueSubmitInfo.pCommandBuffers      = commandBuffers;
//	queueSubmitInfo.signalSemaphoreCount = (uint32_t)signalSemaphores.size();
//	queueSubmitInfo.pSignalSemaphores    = signalSemaphores.data();
//
//	std::array submitInfos = {queueSubmitInfo};
//	ThrowIfFailed(vkQueueSubmit(queue, (uint32_t)submitInfos.size(), submitInfos.data(), signalFence));
//}
//
//void VulkanCBindings::DeviceQueues::QueueSubmit(VkQueue queue, VkCommandBuffer* commandBuffers, uint32_t commandBufferCount, VkPipelineStageFlags waitStageFlags, VkSemaphore waitSemaphore, VkFence signalFence) const
//{
//	std::array waitSemaphores   = {waitSemaphore};
//	std::array waitStages       = {waitStageFlags};
//
//	//TODO: mGPU?
//	//TODO: Performance query?
//	VkSubmitInfo queueSubmitInfo;
//	queueSubmitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
//	queueSubmitInfo.pNext                = nullptr;
//	queueSubmitInfo.waitSemaphoreCount   = (uint32_t)waitSemaphores.size();
//	queueSubmitInfo.pWaitSemaphores      = waitSemaphores.data();
//	queueSubmitInfo.pWaitDstStageMask    = waitStages.data();
//	queueSubmitInfo.commandBufferCount   = commandBufferCount;
//	queueSubmitInfo.pCommandBuffers      = commandBuffers;
//	queueSubmitInfo.signalSemaphoreCount = 0;
//	queueSubmitInfo.pSignalSemaphores    = nullptr;
//
//	std::array submitInfos = {queueSubmitInfo};
//	ThrowIfFailed(vkQueueSubmit(queue, (uint32_t)submitInfos.size(), submitInfos.data(), signalFence));
//}
//
//void VulkanCBindings::DeviceQueues::QueueSubmit(VkQueue queue, VkCommandBuffer* commandBuffers, uint32_t commandBufferCount, VkPipelineStageFlags waitStageFlags, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence signalFence) const
//{
//	std::array waitSemaphores   = {waitSemaphore};
//	std::array waitStages       = {waitStageFlags};
//	std::array signalSemaphores = {signalSemaphore};
//
//	//TODO: mGPU?
//	//TODO: Performance query?
//	VkSubmitInfo queueSubmitInfo;
//	queueSubmitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
//	queueSubmitInfo.pNext                = nullptr;
//	queueSubmitInfo.waitSemaphoreCount   = (uint32_t)waitSemaphores.size();
//	queueSubmitInfo.pWaitSemaphores      = waitSemaphores.data();
//	queueSubmitInfo.pWaitDstStageMask    = waitStages.data();
//	queueSubmitInfo.commandBufferCount   = commandBufferCount;
//	queueSubmitInfo.pCommandBuffers      = commandBuffers;
//	queueSubmitInfo.signalSemaphoreCount = (uint32_t)signalSemaphores.size();
//	queueSubmitInfo.pSignalSemaphores    = signalSemaphores.data();
//
//	std::array submitInfos = {queueSubmitInfo};
//	ThrowIfFailed(vkQueueSubmit(queue, (uint32_t)submitInfos.size(), submitInfos.data(), signalFence));
//}

void D3D12::DeviceQueues::CreateQueueObjects(ID3D12Device* device)
{
	//TODO: mGPU
	D3D12_COMMAND_QUEUE_DESC graphicsQueueDesc;
	graphicsQueueDesc.NodeMask = 0;
	graphicsQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	graphicsQueueDesc.Type     = D3D12_COMMAND_LIST_TYPE_DIRECT;
	graphicsQueueDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
	THROW_IF_FAILED(device->CreateCommandQueue(&graphicsQueueDesc, IID_PPV_ARGS(mGraphicsQueue.put())));

	//TODO: mGPU
	D3D12_COMMAND_QUEUE_DESC computeQueueDesc;
	computeQueueDesc.NodeMask = 0;
	computeQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	computeQueueDesc.Type     = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	computeQueueDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
	THROW_IF_FAILED(device->CreateCommandQueue(&computeQueueDesc, IID_PPV_ARGS(mComputeQueue.put())));

	//TODO: mGPU
	D3D12_COMMAND_QUEUE_DESC copyQueueDesc;
	copyQueueDesc.NodeMask = 0;
	copyQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	copyQueueDesc.Type     = D3D12_COMMAND_LIST_TYPE_COPY;
	copyQueueDesc.Flags    = D3D12_COMMAND_QUEUE_FLAG_NONE;
	THROW_IF_FAILED(device->CreateCommandQueue(&copyQueueDesc, IID_PPV_ARGS(mCopyQueue.put())));
}