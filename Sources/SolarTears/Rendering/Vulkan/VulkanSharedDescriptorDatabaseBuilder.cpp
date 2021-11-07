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

	for(uint32_t layoutBindingIndex = 0; layoutBindingIndex < setBindings.size(); layoutBindingIndex++)
	{
		uint_fast16_t bindingTypeIndex = setBindings[layoutBindingIndex];
		mDatabaseToBuild->mSharedSetFormatsFlat[formatSpan.Begin + layoutBindingIndex] = (SharedDescriptorBindingType)bindingTypeIndex;
	}

	mDatabaseToBuild->mSharedSetCreateMetadatas.push_back(DescriptorDatabase::SharedSetCreateMetadata
	{
		.SetLayout   = setLayout,
		.BindingSpan = formatSpan
	});

	return (uint32_t)(mDatabaseToBuild->mSharedSetCreateMetadatas.size() - 1);
}

void Vulkan::SharedDescriptorDatabaseBuilder::AddSharedSetInfo(uint32_t setMetadataId)
{
	mDatabaseToBuild->mSharedSetRecords.push_back(DescriptorDatabase::SharedSetRecord
	{
		.SetCreateMetadataIndex = setMetadataId,
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
		(uint32_t)samplerManager->GetSamplerVariableArrayLength(),    //Sampler descriptor count
		(uint32_t)scene->mSceneTextures.size(),                       //Texture descriptor count
		scene->GetMaterialCount(),                                    //Material descriptor count
		1u,                                                           //Frame data descriptor count
		scene->GetStaticObjectCount() + scene->GetRigidObjectCount(), //Object data descriptor count
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
		std::array uniformTypes = {SharedDescriptorBindingType::MaterialList, SharedDescriptorBindingType::FrameDataList, SharedDescriptorBindingType::ObjectDataList};

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
		(uint32_t)samplerManager->GetSamplerVariableArrayLength(),   //Samplers binding
		(uint32_t)scene->mSceneTextures.size(),                      //Textures binding
		scene->GetMaterialCount(),                                   //Materials binding
		0u,                                                          //Frame data binding
		scene->GetStaticObjectCount() + scene->GetRigidObjectCount() //Static object datas binding
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
	const uint32_t samplerCount     = 0; //Immutable samplers are used
	const uint32_t textureCount     = (uint32_t)scene->mSceneTextureViews.size();
	const uint32_t materialCount    = scene->GetMaterialCount();
	const uint32_t frameDataCount   = 1u;
	const uint32_t objectDataCount  = scene->GetStaticObjectCount() + scene->GetRigidObjectCount();

	std::array<uint32_t, TotalSharedBindings> bindingDescriptorCounts = std::to_array
	({
		samplerCount,
		textureCount,
		materialCount,
		frameDataCount,
		objectDataCount
	});

	size_t imageDescriptorCount  = 0;
	size_t bufferDescriptorCount = 0;
	for(uint32_t bindingTypeIndex = 0; bindingTypeIndex < TotalSharedBindings; bindingTypeIndex++)
	{
		size_t bindingCount = bindingDescriptorCounts[bindingTypeIndex];

		switch(DescriptorTypesPerSharedBinding[bindingTypeIndex])
		{
		case VK_DESCRIPTOR_TYPE_SAMPLER:
			break;

		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			imageDescriptorCount += bindingCount;
			break;

		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			bufferDescriptorCount += bindingCount;
			break;

		default:
			assert(false); //Unimplemented
			break;
		}
	}

	std::vector<VkDescriptorImageInfo>  imageDescriptorInfos;
	std::vector<VkDescriptorBufferInfo> bufferDescriptorInfos;

	constexpr auto addSamplerBindingsFunc = []([[maybe_unused]] const RenderableScene* sceneToCreateDescriptors, [[maybe_unused]] std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, [[maybe_unused]] std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, [[maybe_unused]] uint32_t samplerIndex)
	{
		//Do nothing
	};

	constexpr auto addTextureBindingFunc = [](const RenderableScene* sceneToCreateDescriptors, std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, [[maybe_unused]] std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, uint32_t textureIndex)
	{
		imageDescriptorInfos.push_back(VkDescriptorImageInfo
		{
			.sampler     = VK_NULL_HANDLE,
			.imageView   = sceneToCreateDescriptors->mSceneTextureViews[textureIndex],
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		});
	};

	constexpr auto addMaterialBindingsFunc = [](const RenderableScene* sceneToCreateDescriptors, [[maybe_unused]] std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, uint32_t materialIndex)
	{
		bufferDescriptorInfos.push_back(VkDescriptorBufferInfo
		{
			.buffer = sceneToCreateDescriptors->mSceneUniformBuffer,
			.offset = sceneToCreateDescriptors->GetMaterialDataOffset(materialIndex),
			.range  = sceneToCreateDescriptors->mMaterialChunkDataSize
		});
	};

	constexpr auto addFrameDataBindingsFunc = [](const RenderableScene* sceneToCreateDescriptors, [[maybe_unused]] std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, [[maybe_unused]] uint32_t bindingIndex)
	{
		bufferDescriptorInfos.push_back(VkDescriptorBufferInfo
		{
			.buffer = sceneToCreateDescriptors->mSceneUniformBuffer,
			.offset = sceneToCreateDescriptors->GetBaseFrameDataOffset(),
			.range  = sceneToCreateDescriptors->mFrameChunkDataSize
		});
	};

	constexpr auto addObjectBindingsFunc = [](const RenderableScene* sceneToCreateDescriptors, [[maybe_unused]] std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, uint32_t objectDataIndex)
	{
		bufferDescriptorInfos.push_back(VkDescriptorBufferInfo
		{
			.buffer = sceneToCreateDescriptors->mSceneUniformBuffer,
			.offset = sceneToCreateDescriptors->GetObjectDataOffset(objectDataIndex),
			.range  = sceneToCreateDescriptors->mObjectChunkDataSize
		});
	};

	using AddBindingFunc = void(*)(const RenderableScene*, std::vector<VkDescriptorImageInfo>&, std::vector<VkDescriptorBufferInfo>&, uint32_t);
	std::array<AddBindingFunc, TotalSharedBindings> bindingAddFuncs;
	bindingAddFuncs[(uint32_t)SharedDescriptorBindingType::SamplerList]    = addSamplerBindingsFunc;
	bindingAddFuncs[(uint32_t)SharedDescriptorBindingType::TextureList]    = addTextureBindingFunc;
	bindingAddFuncs[(uint32_t)SharedDescriptorBindingType::MaterialList]   = addMaterialBindingsFunc;
	bindingAddFuncs[(uint32_t)SharedDescriptorBindingType::FrameDataList]  = addFrameDataBindingsFunc;
	bindingAddFuncs[(uint32_t)SharedDescriptorBindingType::ObjectDataList] = addObjectBindingsFunc;

	std::array<VkDescriptorImageInfo*,  TotalSharedBindings> imageDescriptorInfosPerBinding       = {VK_NULL_HANDLE};
	std::array<VkDescriptorBufferInfo*, TotalSharedBindings> bufferDescriptorInfosPerBinding      = {VK_NULL_HANDLE};
	std::array<VkBufferView*,           TotalSharedBindings> texelBufferDescriptorInfosPerBinding = {VK_NULL_HANDLE};

	imageDescriptorInfos.reserve(imageDescriptorCount);
	bufferDescriptorInfos.reserve(bufferDescriptorCount);
	for(uint32_t bindingTypeIndex = 0; bindingTypeIndex < TotalSharedBindings; bindingTypeIndex++)
	{
		AddBindingFunc addfunc      = bindingAddFuncs[bindingTypeIndex];
		uint32_t       bindingCount = bindingDescriptorCounts[bindingTypeIndex];

		//The memory is already reserved, it's safe to assume it won't move anywhere
		imageDescriptorInfosPerBinding[bindingTypeIndex]       = imageDescriptorInfos.data()  + imageDescriptorInfos.size();
		bufferDescriptorInfosPerBinding[bindingTypeIndex]      = bufferDescriptorInfos.data() + bufferDescriptorInfos.size();
		texelBufferDescriptorInfosPerBinding[bindingTypeIndex] = nullptr;

		for(uint32_t bindingIndex = 0; bindingIndex < bindingCount; bindingIndex++)
		{
			addfunc(scene, imageDescriptorInfos, bufferDescriptorInfos, bindingIndex);
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

			uint32_t bindingTypeIndex = (uint32_t)mDatabaseToBuild->mSharedSetFormatsFlat[bindingIndex];
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
			writeDescriptorSet.pImageInfo       = imageDescriptorInfosPerBinding[bindingTypeIndex];
			writeDescriptorSet.pBufferInfo      = bufferDescriptorInfosPerBinding[bindingTypeIndex];
			writeDescriptorSet.pTexelBufferView = texelBufferDescriptorInfosPerBinding[bindingTypeIndex];

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