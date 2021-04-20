#include "VulkanCScene.hpp"
#include "../VulkanCUtils.hpp"
#include "../VulkanCMemory.hpp"
#include "../VulkanCFunctions.hpp"
#include "../VulkanCShaders.hpp"
#include "../../../../3rdParty/SPIRV-Reflect/spirv_reflect.h"
#include "../../../Core/Scene/SceneDescription.hpp"
#include "../../RenderableSceneMisc.hpp"
#include <array>

VulkanCBindings::RenderableScene::RenderableScene(const VkDevice device, const FrameCounter* frameCounter, const DeviceParameters& deviceParameters, const ShaderManager* shaderManager) :RenderableSceneBase(VulkanUtils::InFlightFrameCount), mFrameCounterRef(frameCounter), mDeviceRef(device)
{
	mSceneVertexBuffer = VK_NULL_HANDLE;
	mSceneIndexBuffer  = VK_NULL_HANDLE;

	mGBufferUniformsDescriptorSetLayout = VK_NULL_HANDLE;
	mGBufferTexturesDescriptorSetLayout = VK_NULL_HANDLE;

	mDescriptorPool = VK_NULL_HANDLE;

	mGBufferUniformsDescriptorSet = VK_NULL_HANDLE;

	mLinearSampler = VK_NULL_HANDLE;

	mBufferMemory  = VK_NULL_HANDLE;
	mTextureMemory = VK_NULL_HANDLE;

	mSceneUniformDataBufferPointer = nullptr;

	mGBufferObjectChunkDataSize = sizeof(RenderableSceneBase::PerObjectData);
	mGBufferFrameChunkDataSize  = sizeof(RenderableSceneBase::PerFrameData);

	mGBufferObjectChunkDataSize = (uint32_t)VulkanUtils::AlignMemory(mGBufferObjectChunkDataSize, deviceParameters.GetDeviceProperties().limits.minUniformBufferOffsetAlignment);
	mGBufferObjectChunkDataSize = (uint32_t)VulkanUtils::AlignMemory(mGBufferObjectChunkDataSize, deviceParameters.GetDeviceProperties().limits.nonCoherentAtomSize);

	mGBufferFrameChunkDataSize = (uint32_t)VulkanUtils::AlignMemory(mGBufferFrameChunkDataSize, deviceParameters.GetDeviceProperties().limits.minUniformBufferOffsetAlignment);
	mGBufferFrameChunkDataSize = (uint32_t)VulkanUtils::AlignMemory(mGBufferFrameChunkDataSize, deviceParameters.GetDeviceProperties().limits.nonCoherentAtomSize);

	CreateSamplers();
	CreateDescriptorSetLayouts(shaderManager);
}

VulkanCBindings::RenderableScene::~RenderableScene()
{
	if(mSceneUniformDataBufferPointer != nullptr)
	{
		vkUnmapMemory(mDeviceRef, mBufferHostVisibleMemory);
		mSceneUniformDataBufferPointer = nullptr;
	}

	SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, mGBufferUniformsDescriptorSetLayout);
	SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, mGBufferTexturesDescriptorSetLayout);

	SafeDestroyObject(vkDestroySampler, mDeviceRef, mLinearSampler);

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

void VulkanCBindings::RenderableScene::FinalizeSceneUpdating()
{
	uint32_t frameResourceIndex = mFrameCounterRef->GetFrameCount() % VulkanUtils::InFlightFrameCount;

	constexpr size_t MaxRangesToFlush = 16;
	std::array<VkMappedMemoryRange, MaxRangesToFlush> mappedMemoryRanges;

	uint32_t        rangesToFlush          = 0;
	uint32_t        lastUpdatedObjectIndex = (uint32_t)(-1);
	uint32_t        freeSpaceIndex         = 0; //We go through all scheduled updates and replace those that have DirtyFrameCount = 0
	SceneUpdateType lastUpdateType         = SceneUpdateType::UPDATE_UNDEFINED;
	for(uint32_t i = 0; i < mScheduledSceneUpdatesCount; i++)
	{
		//TODO: make it without switch statement
		VkDeviceSize bufferOffset  = 0;
		VkDeviceSize updateSize    = 0;
		VkDeviceSize flushSize     = 0;
		void*        updatePointer = nullptr;

		switch(mScheduledSceneUpdates[i].UpdateType)
		{
		case SceneUpdateType::UPDATE_OBJECT:
		{
			bufferOffset  = CalculatePerObjectDataOffset(mScheduledSceneUpdates[i].ObjectIndex, frameResourceIndex);
			updateSize    = sizeof(RenderableSceneBase::PerObjectData);
			flushSize     = mGBufferObjectChunkDataSize;
			updatePointer = &mScenePerObjectData[mScheduledSceneUpdates[i].ObjectIndex];
			break;
		}
		case SceneUpdateType::UPDATE_COMMON:
		{
			bufferOffset  = CalculatePerFrameDataOffset(frameResourceIndex);
			updateSize    = sizeof(RenderableSceneBase::PerFrameData);
			flushSize     = mGBufferFrameChunkDataSize;
			updatePointer = &mScenePerFrameData;
			break;
		}
		default:
		{
			break;
		}
		}

		memcpy((std::byte*)mSceneUniformDataBufferPointer + bufferOffset, updatePointer, updateSize);

		//If update range is not continuous, create new range
		if(lastUpdateType != mScheduledSceneUpdates[i].UpdateType || lastUpdatedObjectIndex != (mScheduledSceneUpdates[i].ObjectIndex - 1))
		{
			//Flush the range blocks
			if(rangesToFlush >= MaxRangesToFlush)
			{
				ThrowIfFailed(vkFlushMappedMemoryRanges(mDeviceRef, rangesToFlush, mappedMemoryRanges.data()));
				rangesToFlush = 0;
			}

			VkMappedMemoryRange memoryMappedRange;
			memoryMappedRange.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			memoryMappedRange.pNext  = nullptr;
			memoryMappedRange.memory = mBufferHostVisibleMemory;
			memoryMappedRange.offset = bufferOffset;
			memoryMappedRange.size   = flushSize;

			mappedMemoryRanges[rangesToFlush] = memoryMappedRange;

			rangesToFlush = rangesToFlush + 1;
		}
		else
		{
			//Append to the last range
			mappedMemoryRanges[rangesToFlush - 1].size += flushSize;
		}

		lastUpdatedObjectIndex = mScheduledSceneUpdates[i].ObjectIndex;
		lastUpdateType         = mScheduledSceneUpdates[i].UpdateType;

		mScheduledSceneUpdates[i].DirtyFramesCount -= 1;

		if(mScheduledSceneUpdates[i].DirtyFramesCount != 0) //Shift all updates by one more element, thus erasing all that have dirty frames count == 0
		{
			//Shift by one
			mScheduledSceneUpdates[freeSpaceIndex] = mScheduledSceneUpdates[i];
			freeSpaceIndex += 1;
		}
	}

	mScheduledSceneUpdatesCount = freeSpaceIndex;

	//Flush the last ranges
	if(rangesToFlush > 0)
	{
		ThrowIfFailed(vkFlushMappedMemoryRanges(mDeviceRef, rangesToFlush, mappedMemoryRanges.data()));
	}
}

void VulkanCBindings::RenderableScene::DrawObjectsOntoGBuffer(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) const
{
	//TODO: extended dynamic state
	std::array sceneVertexBuffers = {mSceneVertexBuffer};

	std::array vertexBufferOffsets = {(VkDeviceSize)0};
	vkCmdBindVertexBuffers(commandBuffer, 0, (uint32_t)(sceneVertexBuffers.size()), sceneVertexBuffers.data(), vertexBufferOffsets.data());

	vkCmdBindIndexBuffer(commandBuffer, mSceneIndexBuffer, 0, VulkanUtils::FormatForIndexType<RenderableSceneIndex>);

	uint32_t frameResourceIndex = mFrameCounterRef->GetFrameCount() % VulkanUtils::InFlightFrameCount;
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

VkDescriptorSetLayout VulkanCBindings::RenderableScene::GetGBufferUniformsDescriptorSetLayout() const
{
	return mGBufferUniformsDescriptorSetLayout;
}

VkDescriptorSetLayout VulkanCBindings::RenderableScene::GetGBufferTexturesDescriptorSetLayout() const
{
	return mGBufferTexturesDescriptorSetLayout;
}

void VulkanCBindings::RenderableScene::CreateSamplers()
{
	//TODO: Fragment density map flags
	//TODO: Cubic filter
	VkSamplerCreateInfo samplerCreateInfo;
	samplerCreateInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.pNext                   = nullptr;
	samplerCreateInfo.flags                   = 0;
	samplerCreateInfo.magFilter               = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter               = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerCreateInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.mipLodBias              = 0.0f;
	samplerCreateInfo.anisotropyEnable        = VK_FALSE;
	samplerCreateInfo.maxAnisotropy           = 0.0f;
	samplerCreateInfo.compareEnable           = VK_FALSE;
	samplerCreateInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
	samplerCreateInfo.minLod                  = 0.0f;
	samplerCreateInfo.maxLod                  = FLT_MAX;
	samplerCreateInfo.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

	ThrowIfFailed(vkCreateSampler(mDeviceRef, &samplerCreateInfo, nullptr, &mLinearSampler));
}

void VulkanCBindings::RenderableScene::CreateDescriptorSetLayouts(const ShaderManager* shaderManager)
{
	std::array gbufferImmutableSamplers = {mLinearSampler};

	std::array gbufferUniformBindingNames = {"UniformBufferObject", "UniformBufferFrame"};
	std::array gbufferTextureBindingNames = {"ObjectTexture"};

	std::vector<VkDescriptorSetLayoutBinding> gbufferUniformsDescriptorSetBindings;
	for(size_t i = 0; i < gbufferUniformBindingNames.size(); i++)
	{
		uint32_t           bindingNumber   = (uint32_t)(-1);
		uint32_t           bindingSet      = (uint32_t)(-1);
		uint32_t           descriptorCount = (uint32_t)(-1);
		VkShaderStageFlags stageFlags      = 0;
		shaderManager->GetGBufferDrawDescriptorBindingInfo(gbufferUniformBindingNames[i], &bindingNumber, &bindingSet, &descriptorCount, &stageFlags, nullptr);

		assert(bindingSet == 1);

		VkDescriptorSetLayoutBinding descriptorSetLayoutBinding;
		descriptorSetLayoutBinding.binding            = bindingNumber;
		descriptorSetLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC; //Only dynamic uniform buffers are used
		descriptorSetLayoutBinding.descriptorCount    = descriptorCount;
		descriptorSetLayoutBinding.stageFlags         = stageFlags;
		descriptorSetLayoutBinding.pImmutableSamplers = nullptr;

		gbufferUniformsDescriptorSetBindings.push_back(descriptorSetLayoutBinding);
	}

	std::vector<VkDescriptorSetLayoutBinding> gbufferTexturesDescriptorSetBindings;
	for(size_t i = 0; i < gbufferTextureBindingNames.size(); i++)
	{
		uint32_t           bindingNumber   = (uint32_t)(-1);
		uint32_t           bindingSet      = (uint32_t)(-1);
		uint32_t           descriptorCount = (uint32_t)(-1);
		VkShaderStageFlags stageFlags      = 0;
		VkDescriptorType   descriptorType  = VK_DESCRIPTOR_TYPE_MAX_ENUM;
		shaderManager->GetGBufferDrawDescriptorBindingInfo(gbufferTextureBindingNames[i], &bindingNumber, &bindingSet, &descriptorCount, &stageFlags, &descriptorType);

		assert(bindingSet == 0);

		VkDescriptorSetLayoutBinding descriptorSetLayoutBinding;
		descriptorSetLayoutBinding.binding            = bindingNumber;
		descriptorSetLayoutBinding.descriptorType     = descriptorType;
		descriptorSetLayoutBinding.descriptorCount    = descriptorCount;
		descriptorSetLayoutBinding.stageFlags         = stageFlags;
		descriptorSetLayoutBinding.pImmutableSamplers = gbufferImmutableSamplers.data(); //Will be ignored if the descriptor type isn't a sampler

		gbufferTexturesDescriptorSetBindings.push_back(descriptorSetLayoutBinding);
	}

	VkDescriptorSetLayoutCreateInfo gbufferUniformsDescriptorSetLayoutCreateInfo;
	gbufferUniformsDescriptorSetLayoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	gbufferUniformsDescriptorSetLayoutCreateInfo.pNext        = nullptr;
	gbufferUniformsDescriptorSetLayoutCreateInfo.flags        = 0;
	gbufferUniformsDescriptorSetLayoutCreateInfo.bindingCount = (uint32_t)(gbufferUniformsDescriptorSetBindings.size());
	gbufferUniformsDescriptorSetLayoutCreateInfo.pBindings    = gbufferUniformsDescriptorSetBindings.data();

	ThrowIfFailed(vkCreateDescriptorSetLayout(mDeviceRef, &gbufferUniformsDescriptorSetLayoutCreateInfo, nullptr, &mGBufferUniformsDescriptorSetLayout));

	VkDescriptorSetLayoutCreateInfo gbufferTexturesDescriptorSetLayoutCreateInfo;
	gbufferTexturesDescriptorSetLayoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	gbufferTexturesDescriptorSetLayoutCreateInfo.pNext        = nullptr;
	gbufferTexturesDescriptorSetLayoutCreateInfo.flags        = 0;
	gbufferTexturesDescriptorSetLayoutCreateInfo.bindingCount = (uint32_t)(gbufferTexturesDescriptorSetBindings.size());
	gbufferTexturesDescriptorSetLayoutCreateInfo.pBindings    = gbufferTexturesDescriptorSetBindings.data();

	ThrowIfFailed(vkCreateDescriptorSetLayout(mDeviceRef, &gbufferTexturesDescriptorSetLayoutCreateInfo, nullptr, &mGBufferTexturesDescriptorSetLayout));
}

VkDeviceSize VulkanCBindings::RenderableScene::CalculatePerObjectDataOffset(uint32_t objectIndex, uint32_t currentFrameResourceIndex)
{
	return mSceneDataUniformObjectBufferOffset + (mScenePerObjectData.size() * currentFrameResourceIndex + objectIndex) * mGBufferObjectChunkDataSize;
}

VkDeviceSize VulkanCBindings::RenderableScene::CalculatePerFrameDataOffset(uint32_t currentFrameResourceIndex)
{
	return mSceneDataUniformFrameBufferOffset + currentFrameResourceIndex * mGBufferFrameChunkDataSize;
}