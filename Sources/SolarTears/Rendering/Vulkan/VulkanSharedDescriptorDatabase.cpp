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

	std::fill(mSceneSetLayouts.begin(), mSceneSetLayouts.end(), (VkDescriptorSetLayout)VK_NULL_HANDLE);
	std::fill(mSceneSets.begin(),       mSceneSets.end(),       (VkDescriptorSet)VK_NULL_HANDLE);
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
	std::fill(mSceneSets.begin(), mSceneSets.end(), (VkDescriptorSet)VK_NULL_HANDLE);

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
	const uint32_t textureCount      = (uint32_t)sceneToCreateDescriptors->mSceneTextureViews.size();
	const uint32_t materialCount     = (uint32_t)(sceneToCreateDescriptors->mMaterialDataSize / sceneToCreateDescriptors->mMaterialChunkDataSize);
	const uint32_t staticObjectCount = (uint32_t)(sceneToCreateDescriptors->mStaticObjectDataSize / sceneToCreateDescriptors->mObjectChunkDataSize);
	const uint32_t frameDataCount    = 1u;
	const uint32_t rigidObjectCount  = (uint32_t)(sceneToCreateDescriptors->mCurrFrameDataToUpdate.size());

	constexpr std::array<uint32_t, SharedDescriptorDatabase::TotalSceneSetLayouts> sceneSetBindingCounts = std::to_array
	({
		1u, //1 binding for textures set
		1u, //1 binding for materials set
		1u, //1 binding for static object data set
		1u, //1 binding for frame data set
		2u  //2 bindings for frame + dynamic object data sets
	});

	constexpr uint32_t UniqueBindings = std::accumulate(sceneSetBindingCounts.begin(), sceneSetBindingCounts.end(), 0);
	constexpr std::array<VkDescriptorType, UniqueBindings> bindingDescriptorTypes = std::to_array
	({
		VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
	});

	std::array<uint32_t, UniqueBindings> bindingDescriptorCounts = std::to_array
	({
		textureCount,
		materialCount,
		staticObjectCount,
		frameDataCount,
		frameDataCount, rigidObjectCount
	});

	size_t imageDescriptorCount  = 0;
	size_t bufferDescriptorCount = 0;
	std::array<VkWriteDescriptorSet, UniqueBindings> writeDescriptorSetTemplates;
	for(uint32_t typeIndex = 0, totalBindingIndex = 0; typeIndex < TotalSceneSetLayouts; typeIndex++)
	{
		for(uint32_t bindingIndex = 0; bindingIndex < sceneSetBindingCounts[typeIndex]; bindingIndex++, totalBindingIndex++)
		{
			//In case we start supporting more types and forget to check here
			assert(bindingDescriptorTypes[totalBindingIndex] == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || bindingDescriptorTypes[totalBindingIndex] == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

			if(bindingDescriptorTypes[totalBindingIndex] == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
			{
				imageDescriptorCount += bindingDescriptorCounts[totalBindingIndex] * SetCountsPerType[typeIndex];
			}

			if(bindingDescriptorTypes[totalBindingIndex] == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
			{
				bufferDescriptorCount += bindingDescriptorCounts[totalBindingIndex] * SetCountsPerType[typeIndex];
			}

			writeDescriptorSetTemplates[totalBindingIndex] =
			{
				.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext            = nullptr,
				.dstSet           = VK_NULL_HANDLE,
				.dstBinding       = bindingIndex,
				.dstArrayElement  = 0,
				.descriptorCount  = bindingDescriptorCounts[totalBindingIndex],
				.descriptorType   = bindingDescriptorTypes[totalBindingIndex],
				.pImageInfo       = nullptr,
				.pBufferInfo      = nullptr,
				.pTexelBufferView = nullptr
			};
		}
	}

	std::vector<VkDescriptorImageInfo> imageDescriptorInfos; 
	imageDescriptorInfos.reserve(imageDescriptorCount);

	std::vector<VkDescriptorBufferInfo> bufferDescriptorInfos;
	bufferDescriptorInfos.reserve(bufferDescriptorCount);

	size_t textureDescriptorOffset      = (size_t)(-1);
	size_t materialDescriptorOffset     = (size_t)(-1);
	size_t staticObjectDescriptorOffset = (size_t)(-1);
	size_t frameDescriptorOffset        = (size_t)(-1);
	size_t rigidObjectDescriptorOffset  = (size_t)(-1);

	//Might've made it so it only gets filled if the corresponding set layouts are not null... Well, it's not like I'll need more bindings than that, and these ones are usually filled up anyway
	textureDescriptorOffset = imageDescriptorInfos.size();
	for(VkImageView sceneImageView: sceneToCreateDescriptors->mSceneTextureViews)
	{
		imageDescriptorInfos.push_back(VkDescriptorImageInfo
		{
			.sampler     = VK_NULL_HANDLE,
			.imageView   = sceneImageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		});
	}

	materialDescriptorOffset = bufferDescriptorInfos.size();
	for(uint32_t materialIndex = 0; materialIndex < materialCount; materialIndex++)
	{
		bufferDescriptorInfos.push_back(VkDescriptorBufferInfo
		{
			.buffer = sceneToCreateDescriptors->mSceneStaticUniformBuffer,
			.offset = sceneToCreateDescriptors->CalculateMaterialDataOffset(materialIndex),
			.range  = sceneToCreateDescriptors->mMaterialChunkDataSize
		});
	}

	staticObjectDescriptorOffset = bufferDescriptorInfos.size();
	for(uint32_t staticObjectIndex = 0; staticObjectIndex < staticObjectCount; staticObjectIndex++)
	{
		bufferDescriptorInfos.push_back(VkDescriptorBufferInfo
		{
			.buffer = sceneToCreateDescriptors->mSceneStaticUniformBuffer,
			.offset = sceneToCreateDescriptors->CalculateStaticObjectDataOffset(staticObjectIndex),
			.range  = sceneToCreateDescriptors->mObjectChunkDataSize
		});
	}

	frameDescriptorOffset = bufferDescriptorInfos.size();
	for(uint32_t frameResourceIndex = 0; frameResourceIndex < Utils::InFlightFrameCount; frameResourceIndex++)
	{
		bufferDescriptorInfos.push_back(VkDescriptorBufferInfo
		{
			.buffer = sceneToCreateDescriptors->mSceneDynamicUniformBuffer,
			.offset = sceneToCreateDescriptors->CalculateFrameDataOffset(frameResourceIndex),
			.range  = sceneToCreateDescriptors->mFrameChunkDataSize
		});
	}

	rigidObjectDescriptorOffset = bufferDescriptorInfos.size();
	for(uint32_t frameResourceIndex = 0; frameResourceIndex < Utils::InFlightFrameCount; frameResourceIndex++)
	{
		for(uint32_t rigidObjectIndex = 0; rigidObjectIndex < rigidObjectCount; rigidObjectIndex++)
		{
			bufferDescriptorInfos.push_back(VkDescriptorBufferInfo
			{
				.buffer = sceneToCreateDescriptors->mSceneDynamicUniformBuffer,
				.offset = sceneToCreateDescriptors->CalculateRigidObjectDataOffset(frameResourceIndex, rigidObjectIndex),
				.range  = sceneToCreateDescriptors->mObjectChunkDataSize
			});
		}
	}

	std::array<VkDescriptorImageInfo*, UniqueBindings> bindingImageStartInfos = std::to_array
	({
		imageDescriptorInfos.data() + textureDescriptorOffset,
		(VkDescriptorImageInfo*)nullptr,
		(VkDescriptorImageInfo*)nullptr,
		(VkDescriptorImageInfo*)nullptr,
		(VkDescriptorImageInfo*)nullptr, (VkDescriptorImageInfo*)nullptr
	});

	std::array<VkDescriptorBufferInfo*, UniqueBindings> bindingBufferStartInfos = std::to_array
	({
		(VkDescriptorBufferInfo*)nullptr,
		bufferDescriptorInfos.data() + materialDescriptorOffset,
		bufferDescriptorInfos.data() + staticObjectDescriptorOffset,
		bufferDescriptorInfos.data() + frameDescriptorOffset,
		bufferDescriptorInfos.data() + frameDescriptorOffset, bufferDescriptorInfos.data() + rigidObjectDescriptorOffset
	});


	constexpr uint32_t TotalBindings = std::inner_product(sceneSetBindingCounts.begin(), sceneSetBindingCounts.end(), SetCountsPerType.begin(), 0);

	std::vector<VkWriteDescriptorSet> writeDescriptorSets;
	writeDescriptorSets.reserve(TotalBindings);

	uint32_t totalBindingOffset = 0;
	for(uint32_t typeIndex = 0; typeIndex < TotalSceneSetLayouts; typeIndex++)
	{
		if(mSceneSetLayouts[typeIndex] != VK_NULL_HANDLE)
		{
			uint32_t layoutSetStart = SetStartIndexPerType((SceneDescriptorSetType)typeIndex);
			for(uint32_t bindingIndex = 0; bindingIndex < (uint32_t)sceneSetBindingCounts[typeIndex]; bindingIndex++)
			{
				VkWriteDescriptorSet writeDescriptorSet = writeDescriptorSetTemplates[bindingIndex];

				VkDescriptorImageInfo*  pImageInfo  = bindingImageStartInfos[(size_t)totalBindingOffset + bindingIndex];
				VkDescriptorBufferInfo* pBufferInfo = bindingBufferStartInfos[(size_t)totalBindingOffset + bindingIndex];
				for(uint32_t setIndex = 0; setIndex < SetCountsPerType[typeIndex]; setIndex++)
				{
					writeDescriptorSet.dstSet      = mSceneSets[(size_t)layoutSetStart + setIndex];
					writeDescriptorSet.pImageInfo  = pImageInfo;
					writeDescriptorSet.pBufferInfo = pBufferInfo;

					writeDescriptorSets.push_back(writeDescriptorSet);

					if(pImageInfo != nullptr)
					{
						//Shift for the next set
						pBufferInfo += writeDescriptorSet.descriptorCount;
					}

					if(pBufferInfo != nullptr)
					{
						//Shift for the next set
						pBufferInfo += writeDescriptorSet.descriptorCount;
					}
				}
			}
		}

		totalBindingOffset += sceneSetBindingCounts[typeIndex];
	}

	vkUpdateDescriptorSets(mDeviceRef, (uint32_t)writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
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