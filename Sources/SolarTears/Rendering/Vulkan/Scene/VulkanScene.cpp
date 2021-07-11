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

	mDescriptorPool = VK_NULL_HANDLE;

	mGBufferMaterialDescriptorSet = VK_NULL_HANDLE;
	std::fill_n(mGBufferUniformsDescriptorSets, Utils::InFlightFrameCount, VK_NULL_HANDLE);

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

	SafeDestroyObject(vkDestroyDescriptorPool, mDeviceRef, mDescriptorPool);

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

void Vulkan::RenderableScene::DrawObjectsOntoGBuffer(VkCommandBuffer commandBuffer, VkPipeline staticPipeline, VkPipeline rigidPipeline) const
{
	//TODO: extended dynamic state
	uint32_t frameResourceIndex = mFrameCounterRef->GetFrameCount() % Utils::InFlightFrameCount;

	std::array sceneVertexBuffers  = {mSceneVertexBuffer};
	std::array vertexBufferOffsets = {(VkDeviceSize)0};
	vkCmdBindVertexBuffers(commandBuffer, 0, (uint32_t)(sceneVertexBuffers.size()), sceneVertexBuffers.data(), vertexBufferOffsets.data());

	vkCmdBindIndexBuffer(commandBuffer, mSceneIndexBuffer, 0, VulkanUtils::FormatForIndexType<RenderableSceneIndex>);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, staticPipeline);

	std::array uniformDescriptorSets = {mGBufferUniformsDescriptorSets[frameResourceIndex]};
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, UniformDescriptorSetIndex, (uint32_t)uniformDescriptorSets.size(), uniformDescriptorSets.data(), 0, nullptr);
	
	std::array materialDescriptorSets = {mGBufferMaterialDescriptorSet};
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, MaterialDescriptorSetIndex, (uint32_t)materialDescriptorSets.size(), materialDescriptorSets.data(), 0, nullptr);

	for(size_t meshIndex = 0; meshIndex < mStaticSceneObjects.size(); meshIndex++)
	{
		for(uint32_t subobjectIndex = mStaticSceneObjects[meshIndex].FirstSubobjectIndex; subobjectIndex < mStaticSceneObjects[meshIndex].AfterLastSubobjectIndex; subobjectIndex++)
		{
			uint32_t materialIndex = mSceneSubobjects[subobjectIndex].MaterialIndex;
			vkCmdPushConstants(commandBuffer, pipelineLayout, MaterialIndexShaderFlags, MaterialIndexOffset, sizeof(uint32_t), &materialIndex);

			vkCmdDrawIndexed(commandBuffer, mSceneSubobjects[subobjectIndex].IndexCount, 1, mSceneSubobjects[subobjectIndex].FirstIndex, mSceneSubobjects[subobjectIndex].VertexOffset, 0);
		}
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rigidPipeline);

	for(size_t meshIndex = 0; meshIndex < mRigidSceneObjects.size(); meshIndex++)
	{
		vkCmdPushConstants(commandBuffer, pipelineLayout, ObjectIndexShaderFlags, ObjectIndexOffset, sizeof(uint32_t), &meshIndex);

		for(uint32_t subobjectIndex = mRigidSceneObjects[meshIndex].FirstSubobjectIndex; subobjectIndex < mRigidSceneObjects[meshIndex].AfterLastSubobjectIndex; subobjectIndex++)
		{
			uint32_t materialIndex = mSceneSubobjects[subobjectIndex].MaterialIndex;
			vkCmdPushConstants(commandBuffer, pipelineLayout, MaterialIndexShaderFlags, MaterialIndexOffset, sizeof(uint32_t), &materialIndex);

			vkCmdDrawIndexed(commandBuffer, mSceneSubobjects[subobjectIndex].IndexCount, 1, mSceneSubobjects[subobjectIndex].FirstIndex, mSceneSubobjects[subobjectIndex].VertexOffset, 0);
		}
	}
}