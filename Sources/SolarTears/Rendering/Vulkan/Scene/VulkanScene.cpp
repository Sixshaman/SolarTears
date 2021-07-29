#include "VulkanScene.hpp"
#include "../VulkanUtils.hpp"
#include "../VulkanMemory.hpp"
#include "../VulkanFunctions.hpp"
#include "../VulkanShaders.hpp"
#include "../../../../3rdParty/SPIRV-Reflect/spirv_reflect.h"
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

void Vulkan::RenderableScene::DrawBakedPositionObjectsWithPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VkShaderStageFlags materialIndexShaderFlags, uint32_t materialIndexOffset) const
{
	for(uint32_t meshIndex = mStaticMeshSpan.Begin; meshIndex < mStaticMeshSpan.End; meshIndex++)
	{
		for(uint32_t submeshIndex = mSceneMeshes[meshIndex].FirstSubmeshIndex; submeshIndex < mSceneMeshes[meshIndex].AfterLastSubmeshIndex; submeshIndex++)
		{
			std::array materialPushConstants = {mSceneSubmeshes[submeshIndex].MaterialIndex};
			vkCmdPushConstants(commandBuffer, pipelineLayout, materialIndexShaderFlags, materialIndexOffset, materialPushConstants.size() * sizeof(decltype(materialPushConstants)::value_type), materialPushConstants.data());

			vkCmdDrawIndexed(commandBuffer, mSceneSubmeshes[submeshIndex].IndexCount, mSceneMeshes[meshIndex].InstanceCount, mSceneSubmeshes[submeshIndex].FirstIndex, mSceneSubmeshes[submeshIndex].VertexOffset, 0);
		}
	}
}

void Vulkan::RenderableScene::DrawBufferedPositionObjectsWithPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VkShaderStageFlags objectIndexShaderFlags, uint32_t objectIndexOffset, VkShaderStageFlags materialIndexShaderFlags, uint32_t materialIndexOffset) const
{
	//The spans should follow each other
	assert(mStaticInstancedMeshSpan.End == mRigidMeshSpan.Begin);

	const uint32_t firstMeshIndex     = mStaticInstancedMeshSpan.Begin;
	const uint32_t afterLastMeshIndex = mRigidMeshSpan.End;

	uint32_t objectDataIndex = 0;
	for(uint32_t meshIndex = firstMeshIndex; meshIndex < afterLastMeshIndex; meshIndex++)
	{
		std::array objectPushConstants = {objectDataIndex};
		vkCmdPushConstants(commandBuffer, pipelineLayout, objectIndexShaderFlags, objectIndexOffset, objectPushConstants.size() * sizeof(decltype(objectPushConstants)::value_type), objectPushConstants.data());

		for(uint32_t submeshIndex = mSceneMeshes[meshIndex].FirstSubmeshIndex; submeshIndex < mSceneMeshes[meshIndex].AfterLastSubmeshIndex; submeshIndex++)
		{
			std::array materialPushConstants = {mSceneSubmeshes[submeshIndex].MaterialIndex};
			vkCmdPushConstants(commandBuffer, pipelineLayout, materialIndexShaderFlags, materialIndexOffset, materialPushConstants.size() * sizeof(decltype(materialPushConstants)::value_type), materialPushConstants.data());

			vkCmdDrawIndexed(commandBuffer, mSceneSubmeshes[submeshIndex].IndexCount, mSceneMeshes[meshIndex].InstanceCount, mSceneSubmeshes[submeshIndex].FirstIndex, mSceneSubmeshes[submeshIndex].VertexOffset, 0);
		}

		objectDataIndex += mSceneMeshes[meshIndex].InstanceCount;
	}
}