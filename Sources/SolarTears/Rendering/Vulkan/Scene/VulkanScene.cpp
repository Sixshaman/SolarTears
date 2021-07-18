#include "VulkanScene.hpp"
#include "../VulkanUtils.hpp"
#include "../VulkanMemory.hpp"
#include "../VulkanFunctions.hpp"
#include "../VulkanShaders.hpp"
#include "../../../../3rdParty/SPIRV-Reflect/spirv_reflect.h"
#include "../../../Core/Scene/SceneDescription.hpp"
#include "../../Common/Scene/RenderableSceneMisc.hpp"
#include "../../Common/RenderingUtils.hpp"
#include <array>

Vulkan::RenderableScene::RenderableScene(const VkDevice device, const FrameCounter* frameCounter, const DeviceParameters& deviceParameters): ModernRenderableScene(frameCounter, VulkanUtils::CalcUniformAlignment(deviceParameters)), mDeviceRef(device)
{
	mSceneVertexBuffer  = VK_NULL_HANDLE;
	mSceneIndexBuffer   = VK_NULL_HANDLE;
	mSceneUniformBuffer = VK_NULL_HANDLE;

	mBufferMemory            = VK_NULL_HANDLE;
	mTextureMemory           = VK_NULL_HANDLE;
	mBufferHostVisibleMemory = VK_NULL_HANDLE;
}

Vulkan::RenderableScene::~RenderableScene()
{
	if(mSceneConstantDataBufferPointer != nullptr)
	{
		vkUnmapMemory(mDeviceRef, mBufferHostVisibleMemory);
	}

	SafeDestroyObject(vkDestroyBuffer, mDeviceRef, mSceneVertexBuffer);
	SafeDestroyObject(vkDestroyBuffer, mDeviceRef, mSceneIndexBuffer);
	SafeDestroyObject(vkDestroyBuffer, mDeviceRef, mSceneUniformBuffer);

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

void Vulkan::RenderableScene::PrepareDrawBuffers(VkCommandBuffer commandBuffer) const
{
	//TODO: extended dynamic state
	uint32_t frameResourceIndex = mFrameCounterRef->GetFrameCount() % Utils::InFlightFrameCount;

	std::array sceneVertexBuffers  = {mSceneVertexBuffer};
	std::array vertexBufferOffsets = {(VkDeviceSize)0};
	vkCmdBindVertexBuffers(commandBuffer, 0, (uint32_t)(sceneVertexBuffers.size()), sceneVertexBuffers.data(), vertexBufferOffsets.data());

	vkCmdBindIndexBuffer(commandBuffer, mSceneIndexBuffer, 0, VulkanUtils::FormatForIndexType<RenderableSceneIndex>);
}

void Vulkan::RenderableScene::DrawStaticObjectsWithRootConstants(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VkShaderStageFlags materialIndexShaderFlags, uint32_t materialIndexOffset) const
{
	for(size_t meshIndex = 0; meshIndex < mStaticSceneObjects.size(); meshIndex++)
	{
		for(uint32_t subobjectIndex = mStaticSceneObjects[meshIndex].FirstSubobjectIndex; subobjectIndex < mStaticSceneObjects[meshIndex].AfterLastSubobjectIndex; subobjectIndex++)
		{
			uint32_t materialIndex = mSceneSubobjects[subobjectIndex].MaterialIndex;
			vkCmdPushConstants(commandBuffer, pipelineLayout, materialIndexShaderFlags, materialIndexOffset, sizeof(uint32_t), &materialIndex);

			vkCmdDrawIndexed(commandBuffer, mSceneSubobjects[subobjectIndex].IndexCount, 1, mSceneSubobjects[subobjectIndex].FirstIndex, mSceneSubobjects[subobjectIndex].VertexOffset, 0);
		}
	}
}

void Vulkan::RenderableScene::DrawRigidObjectsWithRootConstants(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VkShaderStageFlags objectIndexShaderFlags, uint32_t objectIndexOffset, VkShaderStageFlags materialIndexShaderFlags, uint32_t materialIndexOffset) const
{
	for(size_t meshIndex = 0; meshIndex < mRigidSceneObjects.size(); meshIndex++)
	{
		vkCmdPushConstants(commandBuffer, pipelineLayout, objectIndexShaderFlags, objectIndexOffset, sizeof(uint32_t), &meshIndex);

		for(uint32_t subobjectIndex = mRigidSceneObjects[meshIndex].FirstSubobjectIndex; subobjectIndex < mRigidSceneObjects[meshIndex].AfterLastSubobjectIndex; subobjectIndex++)
		{
			uint32_t materialIndex = mSceneSubobjects[subobjectIndex].MaterialIndex;
			vkCmdPushConstants(commandBuffer, pipelineLayout, materialIndexShaderFlags, materialIndexOffset, sizeof(uint32_t), &materialIndex);

			vkCmdDrawIndexed(commandBuffer, mSceneSubobjects[subobjectIndex].IndexCount, 1, mSceneSubobjects[subobjectIndex].FirstIndex, mSceneSubobjects[subobjectIndex].VertexOffset, 0);
		}
	}
}