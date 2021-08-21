#include "VulkanSharedDescriptorDatabase.hpp"
#include "VulkanUtils.hpp"
#include "VulkanFunctions.hpp"
#include "Scene/VulkanScene.hpp"
#include <VulkanGenericStructures.h>

Vulkan::SharedDescriptorDatabase::SharedDescriptorDatabase(const VkDevice device): mDeviceRef(device)
{
	mDescriptorPool = VK_NULL_HANDLE;

	mSamplerDescriptorSetLayout = VK_NULL_HANDLE;
	mSamplerDescriptorSet       = VK_NULL_HANDLE;

	std::fill(mSceneSetLayouts.begin(), mSceneSetLayouts.end(), VK_NULL_HANDLE);
	std::fill(mSceneSets.begin(),       mSceneSets.end(),       VK_NULL_HANDLE);
}

Vulkan::SharedDescriptorDatabase::~SharedDescriptorDatabase()
{
	SafeDestroyObject(vkDestroyDescriptorPool, mDeviceRef, mDescriptorPool);

	SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, mSamplerDescriptorSetLayout);
	for(size_t entryIndex = 0; entryIndex < mSceneSetLayouts.size(); entryIndex++)
	{
		SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, mSceneSetLayouts[entryIndex]);
	}

	for(size_t i = 0; i < mSamplers.size(); i++)
	{
		SafeDestroyObject(vkDestroySampler, mDeviceRef, mSamplers[i]);
	}
}

void Vulkan::SharedDescriptorDatabase::RecreateSets(const RenderableScene* sceneToCreateDescriptors)
{
	RecreateDescriptorPool(sceneToCreateDescriptors);

	AllocateSamplerSet();

	AllocateSceneSets(sceneToCreateDescriptors);
	UpdateSceneSets(sceneToCreateDescriptors);
}

void Vulkan::SharedDescriptorDatabase::RecreateDescriptorPool(const RenderableScene* sceneToCreateDescriptors)
{
	VkDescriptorPoolSize samplerDescriptorPoolSize = {.type = VK_DESCRIPTOR_TYPE_SAMPLER,        .descriptorCount = (uint32_t)mSamplers.size()};
	VkDescriptorPoolSize textureDescriptorPoolSize = {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  .descriptorCount = 0};
	VkDescriptorPoolSize uniformDescriptorPoolSize = {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 0};

	std::array<uint32_t, (size_t)SceneDescriptorSetType::Count> sceneDescriptorCountsPerType = std::to_array
	({
		(uint32_t)sceneToCreateDescriptors->mSceneTextures.size(),                                                    //Texture descriptor count
		(uint32_t)(sceneToCreateDescriptors->mMaterialDataSize / sceneToCreateDescriptors->mMaterialChunkDataSize),   //Material descriptor count
		(uint32_t)(sceneToCreateDescriptors->mStaticObjectDataSize / sceneToCreateDescriptors->mObjectChunkDataSize), //Static data descriptor count
		Utils::InFlightFrameCount,                                                                                    //Frame data descriptor count
		Utils::InFlightFrameCount * (1 + (uint32_t)sceneToCreateDescriptors->mCurrFrameDataToUpdate.size())           //Frame + dynamic data descriptor count
	});

	std::array<VkDescriptorPoolSize*, (size_t)SceneDescriptorSetType::Count> scenePoolSizeRefsPerType = std::to_array
	({
		&textureDescriptorPoolSize, //Texture pool size
		&uniformDescriptorPoolSize, //Material pool size
		&uniformDescriptorPoolSize, //Static data pool size
		&uniformDescriptorPoolSize, //Frame data pool size
		&uniformDescriptorPoolSize  //Frame + dynamic data pool size
	});

	uint32_t setCount = 0;
	for(uint32_t typeIndex = 0; typeIndex < (uint32_t)(SceneDescriptorSetType::Count); typeIndex++)
	{
		if(mSceneSetLayouts[typeIndex] != VK_NULL_HANDLE)
		{
			setCount += SetCountsPerType[typeIndex];
			scenePoolSizeRefsPerType[typeIndex]->descriptorCount += sceneDescriptorCountsPerType[typeIndex];
		}
	}

	std::array poolSizes = {samplerDescriptorPoolSize, textureDescriptorPoolSize, uniformDescriptorPoolSize};

	//TODO: inline uniforms... Though do I really need them?
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
	descriptorPoolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext         = nullptr;
	descriptorPoolCreateInfo.flags         = 0;
	descriptorPoolCreateInfo.maxSets       = setCount;
	descriptorPoolCreateInfo.poolSizeCount = (uint32_t)(poolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes    = poolSizes.data();

	ThrowIfFailed(vkCreateDescriptorPool(mDeviceRef, &descriptorPoolCreateInfo, nullptr, &mDescriptorPool));
}

void Vulkan::SharedDescriptorDatabase::AllocateSceneSets(const RenderableScene* sceneToCreateDescriptors)
{
	std::fill(mSceneSets.begin(), mSceneSets.end(), VK_NULL_HANDLE);

	std::array<uint32_t, (size_t)TotalSceneSetLayouts> variableCountsPerType = std::to_array
	({
		(uint32_t)sceneToCreateDescriptors->mSceneTextures.size(),                                                    //Textures set
		(uint32_t)(sceneToCreateDescriptors->mMaterialDataSize / sceneToCreateDescriptors->mMaterialChunkDataSize),   //Materials set
		(uint32_t)(sceneToCreateDescriptors->mStaticObjectDataSize / sceneToCreateDescriptors->mObjectChunkDataSize), //Static object datas set
		(uint32_t)0,                                                                                                  //Frame data set
		(uint32_t)sceneToCreateDescriptors->mCurrFrameDataToUpdate.size()                                             //Frame data + dynamic object datas set
	});

	std::array<VkDescriptorSetLayout, (size_t)TotalSceneSets> layoutsForSets;
	std::array<uint32_t,              (size_t)TotalSceneSets> variableCountsForSets;

	uint32_t allocatedSetCount = 0;
	for(uint32_t typeIndex = 0; typeIndex < TotalSceneSetLayouts; typeIndex++)
	{
		if(mSceneSetLayouts[typeIndex] != VK_NULL_HANDLE)
		{
			uint32_t layoutSetStart = SetStartIndexPerType((SceneDescriptorSetType)typeIndex);
			uint32_t layoutSetEnd   = layoutSetStart + SetCountsPerType[typeIndex];

			for(uint32_t setIndex = layoutSetStart; setIndex < layoutSetEnd; setIndex++)
			{
				layoutsForSets[allocatedSetCount]        = mSceneSetLayouts[typeIndex];
				variableCountsForSets[allocatedSetCount] = variableCountsPerType[typeIndex];

				allocatedSetCount++;
			}
		}
	}

	VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountAllocateInfo;
	variableCountAllocateInfo.pNext              = nullptr;
	variableCountAllocateInfo.descriptorSetCount = allocatedSetCount;
	variableCountAllocateInfo.pDescriptorCounts  = variableCountsForSets.data();

	vgs::GenericStructureChain<VkDescriptorSetAllocateInfo> descriptorSetAllocateInfoChain;
	descriptorSetAllocateInfoChain.AppendToChain(variableCountAllocateInfo);

	VkDescriptorSetAllocateInfo setAllocateInfo = descriptorSetAllocateInfoChain.GetChainHead();
	setAllocateInfo.descriptorPool     = mDescriptorPool;
	setAllocateInfo.descriptorSetCount = allocatedSetCount;
	setAllocateInfo.pSetLayouts        = layoutsForSets.data();

	std::array<VkDescriptorSet, (size_t)TotalSceneSets> allocatedSets;
	ThrowIfFailed(vkAllocateDescriptorSets(mDeviceRef, &setAllocateInfo, allocatedSets.data()));

	//Remap the sets so SetStartIndexPerType() refers to the correct set
	uint32_t remapSetIndex = 0;
	for(uint32_t typeIndex = 0; typeIndex < TotalSceneSetLayouts; typeIndex++)
	{
		if(mSceneSetLayouts[typeIndex] != VK_NULL_HANDLE)
		{
			uint32_t layoutSetStart = SetStartIndexPerType((SceneDescriptorSetType)typeIndex);
			uint32_t layoutSetEnd   = layoutSetStart + SetCountsPerType[typeIndex];

			for(uint32_t setIndex = layoutSetStart; setIndex < layoutSetEnd; setIndex++)
			{
				mSceneSets[setIndex] = allocatedSets[remapSetIndex];
				remapSetIndex++;
			}
		}
	}
}

void Vulkan::SharedDescriptorDatabase::UpdateSceneSets(const RenderableScene* sceneToCreateDescriptors)
{
	std::vector<VkDescriptorImageInfo> textureDescriptorInfos(sceneToCreateDescriptors->mSceneTextureViews.size());
	for(size_t textureIndex = 0; textureIndex < sceneToCreateDescriptors->mSceneTextureViews.size(); textureIndex++)
	{
		VkDescriptorImageInfo& descriptorImageInfo = textureDescriptorInfos[textureIndex];
		descriptorImageInfo.sampler     = VK_NULL_HANDLE;
		descriptorImageInfo.imageView   = sceneToCreateDescriptors->mSceneTextureViews[textureIndex];
		descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	std::vector<VkWriteDescriptorSet> writeDescriptorSets;
	writeDescriptorSets.reserve(mSceneSets.size());

	for(uint32_t typeIndex = 0; typeIndex < TotalSceneSetLayouts; typeIndex++)
	{
		if(mSceneSetLayouts[typeIndex] != VK_NULL_HANDLE)
		{
			uint32_t layoutSetStart = SetStartIndexPerType((SceneDescriptorSetType)typeIndex);
			uint32_t layoutSetEnd   = layoutSetStart + SetCountsPerType[typeIndex];

			for(uint32_t setIndex = layoutSetStart; setIndex < layoutSetEnd; setIndex++)
			{
				VkWriteDescriptorSet writeDescriptorSet;
				writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSet.pNext = nullptr;
				writeDescriptorSet.dstSet = mSceneSets[setIndex];
				writeDescriptorSet.dstBinding = error;
				writeDescriptorSet.dstArrayElement = 0;
				writeDescriptorSet.descriptorCount = error;
				writeDescriptorSet.descriptorType = error;
				writeDescriptorSet.pImageInfo = error;
				writeDescriptorSet.pBufferInfo = error;
				writeDescriptorSet.pTexelBufferView = error;

				writeDescriptorSets.push_back(writeDescriptorSet);
			}
		}
	}

	vkUpdateDescriptorSets(mDeviceRef, )
}

void Vulkan::SharedDescriptorDatabase::AllocateSamplerSet()
{
	mSamplerDescriptorSet = VK_NULL_HANDLE;

	std::array layouts = {mSamplerDescriptorSetLayout};

	VkDescriptorSetAllocateInfo samplerSetAllocateInfo;
	samplerSetAllocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	samplerSetAllocateInfo.pNext              = nullptr;
	samplerSetAllocateInfo.descriptorPool     = mDescriptorPool;
	samplerSetAllocateInfo.descriptorSetCount = (uint32_t)layouts.size();
	samplerSetAllocateInfo.pSetLayouts        = layouts.data();

	ThrowIfFailed(vkAllocateDescriptorSets(mDeviceRef, &samplerSetAllocateInfo, &mSamplerDescriptorSet));
}

void Vulkan::SharedDescriptorDatabase::CreateSamplers()
{
	std::array samplerAnisotropyFlags  = {VK_FALSE, VK_TRUE};
	std::array samplerAnisotropyLevels = {0.0f,     1.0f};

	constexpr size_t samplerCount = std::tuple_size<decltype(mSamplers)>::value;
	static_assert(samplerCount == samplerAnisotropyFlags.size());
	static_assert(samplerCount == samplerAnisotropyLevels.size());

	for(size_t i = 0; i < mSamplers.size(); i++)
	{
		//TODO: Fragment density map flags
		//TODO: Cubic filter
		VkSamplerCreateInfo samplerCreateInfo;
		samplerCreateInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.pNext                   = nullptr;
		samplerCreateInfo.flags                   = 0;
		samplerCreateInfo.magFilter               = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter               = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.mipLodBias              = 0.0f;
		samplerCreateInfo.anisotropyEnable        = samplerAnisotropyFlags[i];
		samplerCreateInfo.maxAnisotropy           = samplerAnisotropyLevels[i];
		samplerCreateInfo.compareEnable           = VK_FALSE;
		samplerCreateInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
		samplerCreateInfo.minLod                  = 0.0f;
		samplerCreateInfo.maxLod                  = FLT_MAX;
		samplerCreateInfo.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

		ThrowIfFailed(vkCreateSampler(mDeviceRef, &samplerCreateInfo, nullptr, &mSamplers[i]));
	}
}