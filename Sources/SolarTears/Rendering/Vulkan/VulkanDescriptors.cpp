#include "VulkanDescriptors.hpp"
#include "VulkanUtils.hpp"
#include "VulkanFunctions.hpp"
#include "VulkanSamplers.hpp"
#include "Scene/VulkanScene.hpp"
#include <VulkanGenericStructures.h>

Vulkan::DescriptorDatabase::DescriptorDatabase(const VkDevice device): mDeviceRef(device)
{
	mSharedDescriptorPool = VK_NULL_HANDLE;
	mPassDescriptorPool   = VK_NULL_HANDLE;
}

Vulkan::DescriptorDatabase::~DescriptorDatabase()
{
	SafeDestroyObject(vkDestroyDescriptorPool, mDeviceRef, mSharedDescriptorPool);
	for(size_t entryIndex = 0; entryIndex < mSharedSetCreateMetadatas.size(); entryIndex++)
	{
		SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, mSharedSetCreateMetadatas[entryIndex].SetLayout);
	}
}

void Vulkan::DescriptorDatabase::RecreateSharedSets(const RenderableScene* sceneToCreateDescriptors, const SamplerManager* samplerManager)
{
	RecreateSharedDescriptorPool(sceneToCreateDescriptors, samplerManager);

	std::vector<VkDescriptorSet> sharedSets;
	AllocateSharedDescriptorSets(sceneToCreateDescriptors, samplerManager, sharedSets);
	UpdateSharedDescriptorSets(sceneToCreateDescriptors, sharedSets);
}

void Vulkan::DescriptorDatabase::RecreateSharedDescriptorPool(const RenderableScene* sceneToCreateDescriptors, const SamplerManager* samplerManager)
{
	VkDescriptorPoolSize samplerDescriptorPoolSize = {.type = VK_DESCRIPTOR_TYPE_SAMPLER,        .descriptorCount = 0};
	VkDescriptorPoolSize textureDescriptorPoolSize = {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  .descriptorCount = 0};
	VkDescriptorPoolSize uniformDescriptorPoolSize = {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 0};

	std::array<uint32_t, TotalSharedBindings> descriptorCountsPerBinding = std::to_array
	({
		(uint32_t)samplerManager->GetSamplerVariableArrayLength(),                                                    //Sampler descriptor count
		(uint32_t)sceneToCreateDescriptors->mSceneTextures.size(),                                                    //Texture descriptor count
		(uint32_t)(sceneToCreateDescriptors->mMaterialDataSize / sceneToCreateDescriptors->mMaterialChunkDataSize),   //Material descriptor count
		(uint32_t)(sceneToCreateDescriptors->mStaticObjectDataSize / sceneToCreateDescriptors->mObjectChunkDataSize), //Static object data descriptor count
		(uint32_t)sceneToCreateDescriptors->mCurrFrameDataToUpdate.size(),                                            //Dynamic object data descriptor count
		1u                                                                                                            //Frame data descriptor count
	});

	uint32_t totalSetCount = 0;
	for(const SharedSetCreateMetadata& sharedSetMetadata: mSharedSetCreateMetadatas)
	{
		std::array<uint32_t, TotalSharedBindings> bindingCountPerSharedType;
		std::fill(bindingCountPerSharedType.begin(), bindingCountPerSharedType.end(), 0);

		for(uint32_t bindingIndex = sharedSetMetadata.BindingSpan.Begin; bindingIndex < sharedSetMetadata.BindingSpan.End; bindingIndex++)
		{
			uint32_t bindingTypeIndex = (uint32_t)mSharedSetFormatsFlat[bindingIndex];
			bindingCountPerSharedType[bindingTypeIndex] += descriptorCountsPerBinding[bindingTypeIndex] * sharedSetMetadata.SetCount;
		}

		std::array samplerTypes = {SharedDescriptorBindingType::SamplerList};
		std::array textureTypes = {SharedDescriptorBindingType::TextureList};
		std::array uniformTypes = {SharedDescriptorBindingType::MaterialList, SharedDescriptorBindingType::StaticObjectDataList, SharedDescriptorBindingType::MaterialList, SharedDescriptorBindingType::FrameDataList};

		for(SharedDescriptorBindingType samplerType: samplerTypes)
		{
			samplerDescriptorPoolSize.descriptorCount += bindingCountPerSharedType[(uint32_t)samplerType];
		}

		for(SharedDescriptorBindingType textureType: textureTypes)
		{
			textureDescriptorPoolSize.descriptorCount += bindingCountPerSharedType[(uint32_t)textureType];
		}

		for(SharedDescriptorBindingType uniformType: uniformTypes)
		{
			uniformDescriptorPoolSize.descriptorCount += bindingCountPerSharedType[(uint32_t)uniformType];
		}

		totalSetCount += sharedSetMetadata.SetCount;
	}

	std::array poolSizes = {samplerDescriptorPoolSize, textureDescriptorPoolSize, uniformDescriptorPoolSize};

	//TODO: inline uniforms... Though do I really need them?
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
	descriptorPoolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext         = nullptr;
	descriptorPoolCreateInfo.flags         = 0;
	descriptorPoolCreateInfo.maxSets       = totalSetCount;
	descriptorPoolCreateInfo.poolSizeCount = (uint32_t)(poolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes    = poolSizes.data();

	ThrowIfFailed(vkCreateDescriptorPool(mDeviceRef, &descriptorPoolCreateInfo, nullptr, &mSharedDescriptorPool));
}

void Vulkan::DescriptorDatabase::AllocateSharedDescriptorSets(const RenderableScene* sceneToCreateDescriptors, const SamplerManager* samplerManager, std::vector<VkDescriptorSet>& outSharedDescriptorSets)
{
	std::array<uint32_t, TotalSharedBindings> variableCountsPerBindingType = std::to_array
	({
		(uint32_t)samplerManager->GetSamplerVariableArrayLength(),                                                    //Samplers binding
		(uint32_t)sceneToCreateDescriptors->mSceneTextures.size(),                                                    //Textures binding
		(uint32_t)(sceneToCreateDescriptors->mMaterialDataSize / sceneToCreateDescriptors->mMaterialChunkDataSize),   //Materials binding
		(uint32_t)(sceneToCreateDescriptors->mStaticObjectDataSize / sceneToCreateDescriptors->mObjectChunkDataSize), //Static object datas binding
		(uint32_t)sceneToCreateDescriptors->mCurrFrameDataToUpdate.size(),                                            //Dynamic object datas binding
		0u                                                                                                            //Frame data binding
	});

	std::vector<VkDescriptorSetLayout> layoutsForSets;
	std::vector<uint32_t>              variableCountsForSets;
	for(const SharedSetCreateMetadata& sharedSetCreateMetadata: mSharedSetCreateMetadatas)
	{
		uint32_t lastBindingIndex        = (uint32_t)mSharedSetFormatsFlat[sharedSetCreateMetadata.BindingSpan.End - 1];
		uint32_t variableDescriptorCount = variableCountsPerBindingType[lastBindingIndex];

		layoutsForSets.resize(layoutsForSets.size() + sharedSetCreateMetadata.SetCount, sharedSetCreateMetadata.SetLayout);
		variableCountsForSets.resize(layoutsForSets.size() + sharedSetCreateMetadata.SetCount, variableDescriptorCount);
	}

	outSharedDescriptorSets.resize(layoutsForSets.size(), VK_NULL_HANDLE);


	VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountAllocateInfo;
	variableCountAllocateInfo.pNext              = nullptr;
	variableCountAllocateInfo.descriptorSetCount = (uint32_t)variableCountsForSets.size();
	variableCountAllocateInfo.pDescriptorCounts  = variableCountsForSets.data();

	vgs::GenericStructureChain<VkDescriptorSetAllocateInfo> descriptorSetAllocateInfoChain;
	descriptorSetAllocateInfoChain.AppendToChain(variableCountAllocateInfo);

	VkDescriptorSetAllocateInfo& setAllocateInfo = descriptorSetAllocateInfoChain.GetChainHead();
	setAllocateInfo.descriptorPool     = mSharedDescriptorPool;
	setAllocateInfo.descriptorSetCount = (uint32_t)layoutsForSets.size();
	setAllocateInfo.pSetLayouts        = layoutsForSets.data();

	ThrowIfFailed(vkAllocateDescriptorSets(mDeviceRef, &setAllocateInfo, outSharedDescriptorSets.data()));
	

	for(const SharedSetRecord& sharedSetRecord: mSharedSetRecords)
	{
		mDescriptorSets[sharedSetRecord.SetDatabaseIndex] = outSharedDescriptorSets[sharedSetRecord.FlatCreateSetIndex];
	}
}

void Vulkan::DescriptorDatabase::UpdateSharedDescriptorSets(const RenderableScene* sceneToCreateDescriptors, const std::vector<VkDescriptorSet>& sharedDescriptorSets)
{
	const uint32_t samplerCount      = 0; //Immutable samplers are used
	const uint32_t textureCount      = (uint32_t)sceneToCreateDescriptors->mSceneTextureViews.size();
	const uint32_t materialCount     = (uint32_t)(sceneToCreateDescriptors->mMaterialDataSize / sceneToCreateDescriptors->mMaterialChunkDataSize);
	const uint32_t staticObjectCount = (uint32_t)(sceneToCreateDescriptors->mStaticObjectDataSize / sceneToCreateDescriptors->mObjectChunkDataSize);
	const uint32_t rigidObjectCount  = (uint32_t)(sceneToCreateDescriptors->mCurrFrameDataToUpdate.size());
	const uint32_t frameDataCount    = 1u;

	std::array<uint32_t, TotalSharedBindings> bindingDescriptorCounts = std::to_array
	({
		samplerCount,
		textureCount,
		materialCount,
		staticObjectCount,
		rigidObjectCount,
		frameDataCount
	});

	size_t imageDescriptorCount  = 0;
	size_t bufferDescriptorCount = 0;
	for(uint32_t bindingTypeIndex = 0; bindingTypeIndex < TotalSharedBindings; bindingTypeIndex++)
	{
		size_t frameCount   = FrameCountsPerSharedBinding[bindingTypeIndex];
		size_t bindingCount = bindingDescriptorCounts[bindingTypeIndex];

		switch(DescriptorTypesPerSharedBinding[bindingTypeIndex])
		{
		case VK_DESCRIPTOR_TYPE_SAMPLER:
			break;

		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			imageDescriptorCount += bindingCount * frameCount;
			break;

		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			bufferDescriptorCount += bindingCount * frameCount;
			break;

		default:
			assert(false); //Unimplemented
			break;
		}
	}

	std::vector<VkDescriptorImageInfo>  imageDescriptorInfos;
	std::vector<VkDescriptorBufferInfo> bufferDescriptorInfos;

	constexpr auto addSamplerBindingsFunc = [](const RenderableScene* sceneToCreateDescriptors, std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, uint32_t samplerIndex, uint32_t bindingFrameIndex)
	{
		//Do nothing
	};

	constexpr auto addTextureBindingFunc = [](const RenderableScene* sceneToCreateDescriptors, std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, uint32_t textureIndex, uint32_t bindingFrameIndex)
	{
		imageDescriptorInfos.push_back(VkDescriptorImageInfo
		{
			.sampler     = VK_NULL_HANDLE,
			.imageView   = sceneToCreateDescriptors->mSceneTextureViews[textureIndex],
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		});
	};

	constexpr auto addMaterialBindingsFunc = [](const RenderableScene* sceneToCreateDescriptors, std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, uint32_t materialIndex, uint32_t bindingFrameIndex)
	{
		bufferDescriptorInfos.push_back(VkDescriptorBufferInfo
		{
			.buffer = sceneToCreateDescriptors->mSceneStaticUniformBuffer,
			.offset = sceneToCreateDescriptors->CalculateMaterialDataOffset(materialIndex),
			.range  = sceneToCreateDescriptors->mMaterialChunkDataSize
		});
	};

	constexpr auto addStaticObjectBindingsFunc = [](const RenderableScene* sceneToCreateDescriptors, std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, uint32_t staticObjectIndex, uint32_t bindingFrameIndex)
	{
		bufferDescriptorInfos.push_back(VkDescriptorBufferInfo
		{
			.buffer = sceneToCreateDescriptors->mSceneStaticUniformBuffer,
			.offset = sceneToCreateDescriptors->CalculateStaticObjectDataOffset(staticObjectIndex),
			.range  = sceneToCreateDescriptors->mObjectChunkDataSize
		});
	};

	constexpr auto addRigidObjectBindingsFunc = [](const RenderableScene* sceneToCreateDescriptors, std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, uint32_t rigidObjectIndex, uint32_t bindingFrameIndex)
	{
		bufferDescriptorInfos.push_back(VkDescriptorBufferInfo
		{
			.buffer = sceneToCreateDescriptors->mSceneDynamicUniformBuffer,
			.offset = sceneToCreateDescriptors->CalculateRigidObjectDataOffset(bindingFrameIndex, rigidObjectIndex),
			.range  = sceneToCreateDescriptors->mObjectChunkDataSize
		});
	};

	constexpr auto addFrameDataBindingsFunc = [](const RenderableScene* sceneToCreateDescriptors, std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, uint32_t bindingIndex, uint32_t bindingFrameIndex)
	{
		bufferDescriptorInfos.push_back(VkDescriptorBufferInfo
		{
			.buffer = sceneToCreateDescriptors->mSceneDynamicUniformBuffer,
			.offset = sceneToCreateDescriptors->CalculateFrameDataOffset(bindingFrameIndex),
			.range  = sceneToCreateDescriptors->mFrameChunkDataSize
		});
	};

	using AddBindingFunc = void(*)(const RenderableScene*, std::vector<VkDescriptorImageInfo>&, std::vector<VkDescriptorBufferInfo>&, uint32_t, uint32_t);
	constexpr std::array<AddBindingFunc, TotalSharedBindings> bindingAddFuncs = std::to_array
	({
		(AddBindingFunc)addSamplerBindingsFunc,
		(AddBindingFunc)addTextureBindingFunc,
		(AddBindingFunc)addMaterialBindingsFunc,
		(AddBindingFunc)addStaticObjectBindingsFunc,
		(AddBindingFunc)addRigidObjectBindingsFunc,
		(AddBindingFunc)addFrameDataBindingsFunc
	});

	constexpr uint32_t UniqueBindingCount = std::accumulate(FrameCountsPerSharedBinding.begin(), FrameCountsPerSharedBinding.end(), 0u);

	constexpr std::array<uint32_t, TotalSharedBindings> uniqueBindingStarts = { 0 };
	std::exclusive_scan(FrameCountsPerSharedBinding.begin(), FrameCountsPerSharedBinding.end(), uniqueBindingStarts.begin(), 0u);

	std::array<VkDescriptorImageInfo*,  UniqueBindingCount> imageDescriptorInfosPerUniqueBinding       = {0};
	std::array<VkDescriptorBufferInfo*, UniqueBindingCount> bufferDescriptorInfosPerUniqueBinding      = {0};
	std::array<VkBufferView*,           UniqueBindingCount> texelBufferDescriptorInfosPerUniqueBinding = {0};

	imageDescriptorInfos.reserve(imageDescriptorCount);
	bufferDescriptorInfos.reserve(bufferDescriptorCount);
	for(uint32_t bindingTypeIndex = 0; bindingTypeIndex < TotalSharedBindings; bindingTypeIndex++)
	{
		AddBindingFunc addfunc      = bindingAddFuncs[bindingTypeIndex];
		uint32_t       bindingCount = bindingDescriptorCounts[bindingTypeIndex];

		for(uint32_t frameIndex = 0; frameIndex < FrameCountsPerSharedBinding[bindingTypeIndex]; frameIndex++)
		{
			uint32_t uniqueBindingIndex = uniqueBindingStarts[bindingTypeIndex] + frameIndex;

			//The memory is already reserved, it's safe to assume it won't move anywhere
			imageDescriptorInfosPerUniqueBinding[uniqueBindingIndex]       = imageDescriptorInfos.data()  + imageDescriptorInfos.size();
			bufferDescriptorInfosPerUniqueBinding[uniqueBindingIndex]      = bufferDescriptorInfos.data() + bufferDescriptorInfos.size();
			texelBufferDescriptorInfosPerUniqueBinding[uniqueBindingIndex] = nullptr;

			for(uint32_t bindingIndex = 0; bindingIndex < bindingCount; bindingIndex++)
			{
				addfunc(sceneToCreateDescriptors, imageDescriptorInfos, bufferDescriptorInfos, bindingIndex, frameIndex);
			}
		}
	}

	uint32_t totalBindingCount = std::accumulate(mSharedSetCreateMetadatas.begin(), mSharedSetCreateMetadatas.end(), 0, [](const uint32_t acc, const SharedSetCreateMetadata& createMetadata)
	{
		return acc + (createMetadata.BindingSpan.End - createMetadata.BindingSpan.Begin) * createMetadata.SetCount;
	});

	std::vector<VkWriteDescriptorSet> writeDescriptorSets;
	writeDescriptorSets.reserve(totalBindingCount);

	for(const SharedSetCreateMetadata& sharedSetCreateMetadata: mSharedSetCreateMetadatas)
	{
		for(uint32_t bindingIndex = sharedSetCreateMetadata.BindingSpan.Begin; bindingIndex < sharedSetCreateMetadata.BindingSpan.End; bindingIndex++)
		{
			uint32_t layoutBindingIndex = bindingIndex - sharedSetCreateMetadata.BindingSpan.Begin;
			for(uint32_t setIndex = 0; setIndex < sharedSetCreateMetadata.SetCount; setIndex++)
			{
				uint32_t bindingTypeIndex    = (uint32_t)mSharedSetFormatsFlat[bindingIndex];
				uint32_t uniqueBindingIndex = uniqueBindingStarts[bindingTypeIndex] + setIndex % FrameCountsPerSharedBinding[bindingTypeIndex];

				VkWriteDescriptorSet writeDescriptorSet;
				writeDescriptorSet.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSet.pNext            = nullptr;
				writeDescriptorSet.dstSet           = sharedDescriptorSets[setIndex];
				writeDescriptorSet.dstBinding       = layoutBindingIndex;
				writeDescriptorSet.dstArrayElement  = 0;
				writeDescriptorSet.descriptorCount  = bindingDescriptorCounts[bindingTypeIndex];
				writeDescriptorSet.descriptorType   = DescriptorTypesPerSharedBinding[bindingTypeIndex];
				writeDescriptorSet.pImageInfo       = imageDescriptorInfosPerUniqueBinding[uniqueBindingIndex];
				writeDescriptorSet.pBufferInfo      = bufferDescriptorInfosPerUniqueBinding[uniqueBindingIndex];
				writeDescriptorSet.pTexelBufferView = texelBufferDescriptorInfosPerUniqueBinding[uniqueBindingIndex];

				writeDescriptorSets.push_back(writeDescriptorSet);
			}
		}
	}

	vkUpdateDescriptorSets(mDeviceRef, (uint32_t)writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
}