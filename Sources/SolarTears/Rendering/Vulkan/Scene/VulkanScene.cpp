#include "VulkanScene.hpp"
#include "../VulkanUtils.hpp"
#include "../VulkanMemory.hpp"
#include "../VulkanFunctions.hpp"
#include "../VulkanShaders.hpp"
#include "../VulkanDeviceQueues.hpp"
#include "../VulkanWorkerCommandBuffers.hpp"
#include "../../../../3rdParty/SPIRV-Reflect/spirv_reflect.h"
#include "../../Common/Scene/RenderableSceneMisc.hpp"
#include "../../Common/RenderingUtils.hpp"
#include <array>

Vulkan::RenderableScene::RenderableScene(const VkDevice device, const DeviceParameters& deviceParameters): ModernRenderableScene(VulkanUtils::CalcUniformAlignment(deviceParameters)), mDeviceRef(device)
{
	mSceneVertexBuffer  = VK_NULL_HANDLE;
	mSceneIndexBuffer   = VK_NULL_HANDLE;
	mSceneUniformBuffer = VK_NULL_HANDLE;
	mSceneUploadBuffer  = VK_NULL_HANDLE;

	mBufferMemory            = VK_NULL_HANDLE;
	mTextureMemory           = VK_NULL_HANDLE;
	mBufferHostVisibleMemory = VK_NULL_HANDLE;

	for(uint32_t frameIndex = 0; frameIndex < Utils::InFlightFrameCount; frameIndex++)
	{
		VkSemaphoreCreateInfo semaphoreCreateInfo;
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphoreCreateInfo.pNext = nullptr;
		semaphoreCreateInfo.flags = 0;

		ThrowIfFailed(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &mUploadCopySemaphores[frameIndex]));
	}
}

Vulkan::RenderableScene::~RenderableScene()
{
	if(mSceneUploadDataBufferPointer != nullptr)
	{
		vkUnmapMemory(mDeviceRef, mBufferHostVisibleMemory);
	}

	SafeDestroyObject(vkDestroyBuffer, mDeviceRef, mSceneVertexBuffer);
	SafeDestroyObject(vkDestroyBuffer, mDeviceRef, mSceneIndexBuffer);
	SafeDestroyObject(vkDestroyBuffer, mDeviceRef, mSceneUniformBuffer);
	SafeDestroyObject(vkDestroyBuffer, mDeviceRef, mSceneUploadBuffer);

	for(size_t i = 0; i < mSceneTextureViews.size(); i++)
	{
		SafeDestroyObject(vkDestroyImageView, mDeviceRef, mSceneTextureViews[i]);
	}

	for(size_t i = 0; i < mSceneTextures.size(); i++)
	{
		SafeDestroyObject(vkDestroyImage, mDeviceRef, mSceneTextures[i]);
	}

	SafeDestroyObject(vkFreeMemory, mDeviceRef, mTextureMemory);
	SafeDestroyObject(vkFreeMemory, mDeviceRef, mBufferMemory);
	SafeDestroyObject(vkFreeMemory, mDeviceRef, mBufferHostVisibleMemory);
}

void Vulkan::RenderableScene::CopyUploadedSceneObjects(WorkerCommandBuffers* commandBuffers, DeviceQueues* deviceQueues, uint32_t frameResourceIndex)
{
	VkCommandPool   cmdPool   = commandBuffers->GetMainThreadTransferCommandPool(frameResourceIndex);
	VkCommandBuffer cmdBuffer = commandBuffers->GetMainThreadTransferCommandBuffer(frameResourceIndex);

	ThrowIfFailed(vkResetCommandPool(mDeviceRef, cmdPool, 0));

	VkCommandBufferBeginInfo cmdBufferBeginInfo;
	cmdBufferBeginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufferBeginInfo.pNext            = nullptr;
	cmdBufferBeginInfo.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	cmdBufferBeginInfo.pInheritanceInfo = nullptr;

	ThrowIfFailed(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo));

	mCurrFrameUploadCopyRegions[0] = VkBufferCopy
	{
		.srcOffset = GetUploadFrameDataOffset(frameResourceIndex),
		.dstOffset = GetBaseFrameDataOffset(),
		.size      = mFrameChunkDataSize
	};

	for(uint32_t updateIndex = 0; updateIndex < mCurrFrameUpdatedObjectCount; updateIndex++)
	{
		uint32_t meshIndex = mCurrFrameRigidMeshUpdateIndices[updateIndex];
		mCurrFrameUploadCopyRegions[updateIndex + 1] = VkBufferCopy
		{
			.srcOffset = GetUploadRigidObjectDataOffset(frameResourceIndex, meshIndex) - GetStaticObjectCount(),
			.dstOffset = GetObjectDataOffset(meshIndex),
			.size      = mObjectChunkDataSize
		};
	}

	vkCmdCopyBuffer(cmdBuffer, mSceneUploadBuffer, mSceneUniformBuffer, mCurrFrameUpdatedObjectCount + 1, mCurrFrameUploadCopyRegions.data());

	ThrowIfFailed(vkEndCommandBuffer(cmdBuffer));

	deviceQueues->TransferQueueSubmit(cmdBuffer, mUploadCopySemaphores[frameResourceIndex]);
}

VkSemaphore Vulkan::RenderableScene::GetUploadSemaphore(uint32_t frameResourceIndex) const
{
	return mUploadCopySemaphores[frameResourceIndex];
}

void Vulkan::RenderableScene::PrepareDrawBuffers(VkCommandBuffer commandBuffer) const
{
	//TODO: extended dynamic state
	std::array sceneVertexBuffers  = {mSceneVertexBuffer};
	std::array vertexBufferOffsets = {(VkDeviceSize)0};
	vkCmdBindVertexBuffers(commandBuffer, 0, (uint32_t)(sceneVertexBuffers.size()), sceneVertexBuffers.data(), vertexBufferOffsets.data());

	vkCmdBindIndexBuffer(commandBuffer, mSceneIndexBuffer, 0, VulkanUtils::FormatForIndexType<RenderableSceneIndex>);
}