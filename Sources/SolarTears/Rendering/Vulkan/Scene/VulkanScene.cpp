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
#include <numeric>

Vulkan::RenderableScene::RenderableScene(const VkDevice device, const FrameCounter* frameCounter, const DeviceParameters& deviceParameters): ModernRenderableScene(frameCounter), mDeviceRef(device)
{
	mSceneVertexBuffer  = VK_NULL_HANDLE;
	mSceneIndexBuffer   = VK_NULL_HANDLE;
	mSceneUniformBuffer = VK_NULL_HANDLE;

	mDescriptorPool = VK_NULL_HANDLE;

	mGBufferUniformsDescriptorSet = VK_NULL_HANDLE;

	mBufferMemory            = VK_NULL_HANDLE;
	mTextureMemory           = VK_NULL_HANDLE;
	mBufferHostVisibleMemory = VK_NULL_HANDLE;

	mCBufferAlignmentSize = std::lcm(deviceParameters.GetDeviceProperties().limits.minUniformBufferOffsetAlignment, deviceParameters.GetDeviceProperties().limits.nonCoherentAtomSize);

	Init();
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

void Vulkan::RenderableScene::DrawObjectsOntoGBuffer(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) const
{
	//TODO: extended dynamic state
	std::array sceneVertexBuffers = {mSceneVertexBuffer};

	std::array vertexBufferOffsets = {(VkDeviceSize)0};
	vkCmdBindVertexBuffers(commandBuffer, 0, (uint32_t)(sceneVertexBuffers.size()), sceneVertexBuffers.data(), vertexBufferOffsets.data());

	vkCmdBindIndexBuffer(commandBuffer, mSceneIndexBuffer, 0, VulkanUtils::FormatForIndexType<RenderableSceneIndex>);

	uint32_t frameResourceIndex = mFrameCounterRef->GetFrameCount() % Utils::InFlightFrameCount;
	uint32_t PerFrameOffset     = frameResourceIndex * mGBufferFrameChunkDataSize;

	for(size_t meshIndex = 0; meshIndex < mSceneMeshes.size(); meshIndex++)
	{
		uint32_t PerObjectOffset = (uint32_t)(frameResourceIndex * mSceneMeshes.size() + meshIndex) * mGBufferObjectChunkDataSize;

		for(uint32_t subobjectIndex = mSceneMeshes[meshIndex].FirstSubobjectIndex; subobjectIndex < mSceneMeshes[meshIndex].AfterLastSubobjectIndex; subobjectIndex++)
		{
			//Bind PUSH CONSTANTS for material index

			//TODO: bindless
			//Textures go first, buffers go last (because they are supposed to update frequently, doesn't matter for now)
			//Buffers are go in order: per-object binding, per-frame binding
			std::array descriptorSets = {mGBufferTextureDescriptorSets[mSceneSubobjects[subobjectIndex].TextureDescriptorSetIndex], mGBufferUniformsDescriptorSet};
			std::array dynamicOffsets = {PerObjectOffset, PerFrameOffset};

			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, (uint32_t)descriptorSets.size(), descriptorSets.data(), (uint32_t)dynamicOffsets.size(), dynamicOffsets.data());

			vkCmdDrawIndexed(commandBuffer, mSceneSubobjects[subobjectIndex].IndexCount, 1, mSceneSubobjects[subobjectIndex].FirstIndex, mSceneSubobjects[subobjectIndex].VertexOffset, 0);
		}
	}
}