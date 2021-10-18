#include "VulkanSharedDescriptorDatabaseBuilder.hpp"
#include "VulkanDescriptors.hpp"
#include "VulkanSamplers.hpp"
#include "Scene/VulkanScene.hpp"
#include "VulkanUtils.hpp"
#include "VulkanFunctions.hpp"
#include <VulkanGenericStructures.h>
#include <span>
#include <vector>
#include <algorithm>

Vulkan::SharedDescriptorDatabaseBuilder::SharedDescriptorDatabaseBuilder(DescriptorDatabase* databaseToBuild): mDatabaseToBuild(databaseToBuild)
{
}

Vulkan::SharedDescriptorDatabaseBuilder::~SharedDescriptorDatabaseBuilder()
{
}

void Vulkan::SharedDescriptorDatabaseBuilder::ClearRegisteredSharedSetInfos()
{
	mDatabaseToBuild->mSharedSetCreateMetadatas.clear();
	mDatabaseToBuild->mSharedSetFormatsFlat.clear();
}

uint32_t Vulkan::SharedDescriptorDatabaseBuilder::RegisterSharedSetInfo(VkDescriptorSetLayout setLayout, std::span<const uint16_t> setBindings)
{
	Span<uint32_t> formatSpan =
	{
		.Begin = (uint32_t)mDatabaseToBuild->mSharedSetFormatsFlat.size(),
		.End   = (uint32_t)(mDatabaseToBuild->mSharedSetFormatsFlat.size() + setBindings.size())
	};

	mDatabaseToBuild->mSharedSetFormatsFlat.resize(mDatabaseToBuild->mSharedSetFormatsFlat.size() + setBindings.size());
		
	uint32_t layoutFrameCount = 1;
	for(uint32_t layoutBindingIndex = 0; layoutBindingIndex < setBindings.size(); layoutBindingIndex++)
	{
		uint_fast16_t bindingTypeIndex = setBindings[layoutBindingIndex];
		layoutFrameCount = std::lcm(layoutFrameCount, FrameCountsPerSharedBinding[bindingTypeIndex]);

		mDatabaseToBuild->mSharedSetFormatsFlat[formatSpan.Begin + layoutBindingIndex] = (SharedDescriptorBindingType)bindingTypeIndex;
	}

	uint32_t setLayoutStart = (uint32_t)mDatabaseToBuild->mSharedSetCreateMetadatas.size();
	for(uint32_t frameIndex = 0; frameIndex < layoutFrameCount; frameIndex++)
	{
		mDatabaseToBuild->mSharedSetCreateMetadatas.push_back(DescriptorDatabase::SharedSetCreateMetadata
		{
			.SetLayout     = setLayout,
			.SetFrameIndex = frameIndex,
			.BindingSpan   = formatSpan
		});
	}

	mSetMetadataSpans.push_back(SetCreateMetadataSpan
	{
		.BaseIndex  = setLayoutStart,
		.FrameCount = layoutFrameCount
	});

	return (uint32_t)(mSetMetadataSpans.size() - 1);
}

void Vulkan::SharedDescriptorDatabaseBuilder::AddSharedSetInfo(uint32_t setSpanId, uint32_t frame)
{
	const SetCreateMetadataSpan& setMetadataSpan = mSetMetadataSpans[setSpanId];
	uint32_t setMetadataIndex = setMetadataSpan.BaseIndex + frame % setMetadataSpan.FrameCount;

	mDatabaseToBuild->mSharedSetRecords.push_back(DescriptorDatabase::SharedSetRecord
	{
		.SetCreateMetadataIndex = setMetadataIndex,
		.SetDatabaseIndex       = (uint32_t)mDatabaseToBuild->mDescriptorSets.size()
	});

	mDatabaseToBuild->mDescriptorSets.push_back(VK_NULL_HANDLE);
}

void Vulkan::SharedDescriptorDatabaseBuilder::RecreateSharedSets(const RenderableScene* scene, const SamplerManager* samplerManager)
{
	RecreateSharedDescriptorPool(scene, samplerManager);

	std::vector<VkDescriptorSet> setsPerLayout;
	AllocateSharedDescriptorSets(scene, samplerManager, setsPerLayout);

	UpdateSharedDescriptorSets(scene, setsPerLayout);
	AssignSharedDescriptorSets(setsPerLayout);
}

void Vulkan::SharedDescriptorDatabaseBuilder::RecreateSharedDescriptorPool(const RenderableScene* scene, const SamplerManager* samplerManager)
{
	SafeDestroyObject(vkDestroyDescriptorPool, mDatabaseToBuild->mDeviceRef, mDatabaseToBuild->mSharedDescriptorPool);

	if(mDatabaseToBuild->mSharedSetCreateMetadatas.empty())
	{
		return;
	}

	VkDescriptorPoolSize samplerDescriptorPoolSize = {.type = VK_DESCRIPTOR_TYPE_SAMPLER,        .descriptorCount = 0};
	VkDescriptorPoolSize textureDescriptorPoolSize = {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  .descriptorCount = 0};
	VkDescriptorPoolSize uniformDescriptorPoolSize = {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 0};

	std::array<uint32_t, TotalSharedBindings> descriptorCountsPerBinding = std::to_array
	({
		(uint32_t)samplerManager->GetSamplerVariableArrayLength(),              //Sampler descriptor count
		(uint32_t)scene->mSceneTextures.size(),                                 //Texture descriptor count
		(uint32_t)(scene->mMaterialDataSize / scene->mMaterialChunkDataSize),   //Material descriptor count
		(uint32_t)(scene->mStaticObjectDataSize / scene->mObjectChunkDataSize), //Static object data descriptor count
		(uint32_t)scene->mCurrFrameDataToUpdate.size(),                         //Dynamic object data descriptor count
		1u                                                                      //Frame data descriptor count
	});

	for(const DescriptorDatabase::SharedSetCreateMetadata& sharedSetMetadata: mDatabaseToBuild->mSharedSetCreateMetadatas)
	{
		std::array<uint32_t, TotalSharedBindings> bindingCountPerSharedType;
		std::fill(bindingCountPerSharedType.begin(), bindingCountPerSharedType.end(), 0);

		for(uint32_t bindingIndex = sharedSetMetadata.BindingSpan.Begin; bindingIndex < sharedSetMetadata.BindingSpan.End; bindingIndex++)
		{
			uint32_t bindingTypeIndex = (uint32_t)mDatabaseToBuild->mSharedSetFormatsFlat[bindingIndex];
			bindingCountPerSharedType[bindingTypeIndex] += descriptorCountsPerBinding[bindingTypeIndex];
		}

		std::array samplerTypes = {SharedDescriptorBindingType::SamplerList};
		std::array textureTypes = {SharedDescriptorBindingType::TextureList};
		std::array uniformTypes = {SharedDescriptorBindingType::MaterialList, SharedDescriptorBindingType::StaticObjectDataList, SharedDescriptorBindingType::DynamicObjectDataList, SharedDescriptorBindingType::FrameDataList};

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
	}

	uint32_t poolSizeCount = 0;
	std::array<VkDescriptorPoolSize, 3> poolSizes;
	for(const VkDescriptorPoolSize& poolSize: {samplerDescriptorPoolSize, textureDescriptorPoolSize, uniformDescriptorPoolSize})
	{
		if(poolSize.descriptorCount != 0)
		{
			poolSizes[poolSizeCount] = poolSize;
			poolSizeCount++;
		}
	}

	//TODO: inline uniforms... Though do I really need them?
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
	descriptorPoolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext         = nullptr;
	descriptorPoolCreateInfo.flags         = 0;
	descriptorPoolCreateInfo.maxSets       = (uint32_t)mDatabaseToBuild->mSharedSetCreateMetadatas.size();
	descriptorPoolCreateInfo.poolSizeCount = poolSizeCount;
	descriptorPoolCreateInfo.pPoolSizes    = poolSizes.data();

	ThrowIfFailed(vkCreateDescriptorPool(mDatabaseToBuild->mDeviceRef, &descriptorPoolCreateInfo, nullptr, &mDatabaseToBuild->mSharedDescriptorPool));
}

void Vulkan::SharedDescriptorDatabaseBuilder::AllocateSharedDescriptorSets(const RenderableScene* scene, const SamplerManager* samplerManager, std::vector<VkDescriptorSet>& outAllocatedSets)
{
	outAllocatedSets.resize(mDatabaseToBuild->mSharedSetCreateMetadatas.size(), VK_NULL_HANDLE);

	if(mDatabaseToBuild->mSharedSetCreateMetadatas.empty())
	{
		return;
	}

	std::array<uint32_t, TotalSharedBindings> variableCountsPerBindingType = std::to_array
	({
		(uint32_t)samplerManager->GetSamplerVariableArrayLength(),              //Samplers binding
		(uint32_t)scene->mSceneTextures.size(),                                 //Textures binding
		(uint32_t)(scene->mMaterialDataSize / scene->mMaterialChunkDataSize),   //Materials binding
		(uint32_t)(scene->mStaticObjectDataSize / scene->mObjectChunkDataSize), //Static object datas binding
		(uint32_t)scene->mCurrFrameDataToUpdate.size(),                         //Dynamic object datas binding
		0u                                                                      //Frame data binding
	});

	std::vector<VkDescriptorSetLayout> layoutsForSets;
	std::vector<uint32_t>              variableCountsForSets;
	for(const DescriptorDatabase::SharedSetCreateMetadata& sharedSetCreateMetadata: mDatabaseToBuild->mSharedSetCreateMetadatas)
	{
		uint32_t lastBindingIndex        = (uint32_t)mDatabaseToBuild->mSharedSetFormatsFlat[sharedSetCreateMetadata.BindingSpan.End - 1];
		uint32_t variableDescriptorCount = variableCountsPerBindingType[lastBindingIndex];

		layoutsForSets.push_back(sharedSetCreateMetadata.SetLayout);
		variableCountsForSets.push_back(variableDescriptorCount);
	}

	VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountAllocateInfo;
	variableCountAllocateInfo.pNext              = nullptr;
	variableCountAllocateInfo.descriptorSetCount = (uint32_t)variableCountsForSets.size();
	variableCountAllocateInfo.pDescriptorCounts  = variableCountsForSets.data();

	vgs::GenericStructureChain<VkDescriptorSetAllocateInfo> descriptorSetAllocateInfoChain;
	descriptorSetAllocateInfoChain.AppendToChain(variableCountAllocateInfo);

	VkDescriptorSetAllocateInfo& setAllocateInfo = descriptorSetAllocateInfoChain.GetChainHead();
	setAllocateInfo.descriptorPool     = mDatabaseToBuild->mSharedDescriptorPool;
	setAllocateInfo.descriptorSetCount = (uint32_t)layoutsForSets.size();
	setAllocateInfo.pSetLayouts        = layoutsForSets.data();

	ThrowIfFailed(vkAllocateDescriptorSets(mDatabaseToBuild->mDeviceRef, &setAllocateInfo, outAllocatedSets.data()));
}

void Vulkan::SharedDescriptorDatabaseBuilder::UpdateSharedDescriptorSets(const RenderableScene* scene, const std::vector<VkDescriptorSet>& setsToWritePerCreateMetadata)
{
	const uint32_t samplerCount      = 0; //Immutable samplers are used
	const uint32_t textureCount      = (uint32_t)scene->mSceneTextureViews.size();
	const uint32_t materialCount     = (uint32_t)(scene->mMaterialDataSize / scene->mMaterialChunkDataSize);
	const uint32_t staticObjectCount = (uint32_t)(scene->mStaticObjectDataSize / scene->mObjectChunkDataSize);
	const uint32_t rigidObjectCount  = (uint32_t)(scene->mCurrFrameDataToUpdate.size());
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

	constexpr auto addSamplerBindingsFunc = []([[maybe_unused]] const RenderableScene* sceneToCreateDescriptors, [[maybe_unused]] std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, [[maybe_unused]] std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, [[maybe_unused]] uint32_t samplerIndex, [[maybe_unused]] uint32_t bindingFrameIndex)
	{
		//Do nothing
	};

	constexpr auto addTextureBindingFunc = [](const RenderableScene* sceneToCreateDescriptors, std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, [[maybe_unused]] std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, uint32_t textureIndex, [[maybe_unused]] uint32_t bindingFrameIndex)
	{
		imageDescriptorInfos.push_back(VkDescriptorImageInfo
		{
			.sampler     = VK_NULL_HANDLE,
			.imageView   = sceneToCreateDescriptors->mSceneTextureViews[textureIndex],
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		});
	};

	constexpr auto addMaterialBindingsFunc = [](const RenderableScene* sceneToCreateDescriptors, [[maybe_unused]] std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, uint32_t materialIndex, [[maybe_unused]] uint32_t bindingFrameIndex)
	{
		bufferDescriptorInfos.push_back(VkDescriptorBufferInfo
		{
			.buffer = sceneToCreateDescriptors->mSceneStaticUniformBuffer,
			.offset = sceneToCreateDescriptors->CalculateMaterialDataOffset(materialIndex),
			.range  = sceneToCreateDescriptors->mMaterialChunkDataSize
		});
	};

	constexpr auto addStaticObjectBindingsFunc = [](const RenderableScene* sceneToCreateDescriptors, [[maybe_unused]] std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, uint32_t staticObjectIndex, [[maybe_unused]] uint32_t bindingFrameIndex)
	{
		bufferDescriptorInfos.push_back(VkDescriptorBufferInfo
		{
			.buffer = sceneToCreateDescriptors->mSceneStaticUniformBuffer,
			.offset = sceneToCreateDescriptors->CalculateStaticObjectDataOffset(staticObjectIndex),
			.range  = sceneToCreateDescriptors->mObjectChunkDataSize
		});
	};

	constexpr auto addRigidObjectBindingsFunc = [](const RenderableScene* sceneToCreateDescriptors, [[maybe_unused]] std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, uint32_t rigidObjectIndex, uint32_t bindingFrameIndex)
	{
		bufferDescriptorInfos.push_back(VkDescriptorBufferInfo
		{
			.buffer = sceneToCreateDescriptors->mSceneDynamicUniformBuffer,
			.offset = sceneToCreateDescriptors->CalculateRigidObjectDataOffset(bindingFrameIndex, rigidObjectIndex),
			.range  = sceneToCreateDescriptors->mObjectChunkDataSize
		});
	};

	constexpr auto addFrameDataBindingsFunc = [](const RenderableScene* sceneToCreateDescriptors, [[maybe_unused]] std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, [[maybe_unused]] uint32_t bindingIndex, uint32_t bindingFrameIndex)
	{
		bufferDescriptorInfos.push_back(VkDescriptorBufferInfo
		{
			.buffer = sceneToCreateDescriptors->mSceneDynamicUniformBuffer,
			.offset = sceneToCreateDescriptors->CalculateFrameDataOffset(bindingFrameIndex),
			.range  = sceneToCreateDescriptors->mFrameChunkDataSize
		});
	};

	using AddBindingFunc = void(*)(const RenderableScene*, std::vector<VkDescriptorImageInfo>&, std::vector<VkDescriptorBufferInfo>&, uint32_t, uint32_t);
	std::array<AddBindingFunc, TotalSharedBindings> bindingAddFuncs;
	bindingAddFuncs[(uint32_t)SharedDescriptorBindingType::SamplerList]           = addSamplerBindingsFunc;
	bindingAddFuncs[(uint32_t)SharedDescriptorBindingType::TextureList]           = addTextureBindingFunc;
	bindingAddFuncs[(uint32_t)SharedDescriptorBindingType::MaterialList]          = addMaterialBindingsFunc;
	bindingAddFuncs[(uint32_t)SharedDescriptorBindingType::StaticObjectDataList]  = addStaticObjectBindingsFunc;
	bindingAddFuncs[(uint32_t)SharedDescriptorBindingType::DynamicObjectDataList] = addRigidObjectBindingsFunc;
	bindingAddFuncs[(uint32_t)SharedDescriptorBindingType::FrameDataList]         = addFrameDataBindingsFunc;

	constexpr uint32_t UniqueBindingCount = std::accumulate(FrameCountsPerSharedBinding.begin(), FrameCountsPerSharedBinding.end(), 0u);

	std::array<uint32_t, TotalSharedBindings> uniqueBindingStarts = {0};
	std::exclusive_scan(FrameCountsPerSharedBinding.begin(), FrameCountsPerSharedBinding.end(), uniqueBindingStarts.begin(), 0u);

	std::array<VkDescriptorImageInfo*,  UniqueBindingCount> imageDescriptorInfosPerUniqueBinding       = {VK_NULL_HANDLE};
	std::array<VkDescriptorBufferInfo*, UniqueBindingCount> bufferDescriptorInfosPerUniqueBinding      = {VK_NULL_HANDLE};
	std::array<VkBufferView*,           UniqueBindingCount> texelBufferDescriptorInfosPerUniqueBinding = {VK_NULL_HANDLE};

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
				addfunc(scene, imageDescriptorInfos, bufferDescriptorInfos, bindingIndex, frameIndex);
			}
		}
	}

	uint32_t totalBindingCount = std::accumulate(mDatabaseToBuild->mSharedSetCreateMetadatas.begin(), mDatabaseToBuild->mSharedSetCreateMetadatas.end(), 0, [](const uint32_t acc, const DescriptorDatabase::SharedSetCreateMetadata& createMetadata)
	{
		return acc + (createMetadata.BindingSpan.End - createMetadata.BindingSpan.Begin);
	});

	std::vector<VkWriteDescriptorSet> writeDescriptorSets;
	writeDescriptorSets.reserve(totalBindingCount);

	for(uint32_t setMetadataIndex = 0; setMetadataIndex < mDatabaseToBuild->mSharedSetCreateMetadatas.size(); setMetadataIndex++)
	{
		const DescriptorDatabase::SharedSetCreateMetadata& sharedSetCreateMetadata = mDatabaseToBuild->mSharedSetCreateMetadatas[setMetadataIndex];
		for(uint32_t bindingIndex = sharedSetCreateMetadata.BindingSpan.Begin; bindingIndex < sharedSetCreateMetadata.BindingSpan.End; bindingIndex++)
		{
			uint32_t layoutBindingIndex = bindingIndex - sharedSetCreateMetadata.BindingSpan.Begin;

			uint32_t bindingTypeIndex   = (uint32_t)mDatabaseToBuild->mSharedSetFormatsFlat[bindingIndex];
			uint32_t uniqueBindingIndex = uniqueBindingStarts[bindingTypeIndex] + sharedSetCreateMetadata.SetFrameIndex % FrameCountsPerSharedBinding[bindingTypeIndex];

			if(bindingDescriptorCounts[bindingTypeIndex] == 0)
			{
				continue;
			}

			VkWriteDescriptorSet writeDescriptorSet;
			writeDescriptorSet.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.pNext            = nullptr;
			writeDescriptorSet.dstSet           = setsToWritePerCreateMetadata[setMetadataIndex];
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

	vkUpdateDescriptorSets(mDatabaseToBuild->mDeviceRef, (uint32_t)writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
}

void Vulkan::SharedDescriptorDatabaseBuilder::AssignSharedDescriptorSets(const std::vector<VkDescriptorSet>& setsPerCreateMetadata)
{
	for(const DescriptorDatabase::SharedSetRecord& sharedSetRecord: mDatabaseToBuild->mSharedSetRecords)
	{
		mDatabaseToBuild->mDescriptorSets[sharedSetRecord.SetDatabaseIndex] = setsPerCreateMetadata[sharedSetRecord.SetCreateMetadataIndex];
	}
}