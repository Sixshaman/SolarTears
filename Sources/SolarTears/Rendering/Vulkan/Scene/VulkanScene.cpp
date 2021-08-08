#include "VulkanScene.hpp"
#include "../VulkanUtils.hpp"
#include "../VulkanMemory.hpp"
#include "../VulkanFunctions.hpp"
#include "../VulkanShaders.hpp"
#include "../../../../3rdParty/SPIRV-Reflect/spirv_reflect.h"
#include "../../Common/Scene/RenderableSceneMisc.hpp"
#include "../../Common/RenderingUtils.hpp"
#include <array>

Vulkan::RenderableScene::RenderableScene(const VkDevice device, const DeviceParameters& deviceParameters): ModernRenderableScene(VulkanUtils::CalcUniformAlignment(deviceParameters)), mDeviceRef(device)
{
	mSceneVertexBuffer         = VK_NULL_HANDLE;
	mSceneIndexBuffer          = VK_NULL_HANDLE;
	mSceneStaticUniformBuffer  = VK_NULL_HANDLE;
	mSceneDynamicUniformBuffer = VK_NULL_HANDLE;

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
	SafeDestroyObject(vkDestroyBuffer, mDeviceRef, mSceneStaticUniformBuffer);
	SafeDestroyObject(vkDestroyBuffer, mDeviceRef, mSceneDynamicUniformBuffer);

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
	std::array sceneVertexBuffers  = {mSceneVertexBuffer};
	std::array vertexBufferOffsets = {(VkDeviceSize)0};
	vkCmdBindVertexBuffers(commandBuffer, 0, (uint32_t)(sceneVertexBuffers.size()), sceneVertexBuffers.data(), vertexBufferOffsets.data());

	vkCmdBindIndexBuffer(commandBuffer, mSceneIndexBuffer, 0, VulkanUtils::FormatForIndexType<RenderableSceneIndex>);
}