#include "VulkanSceneDescriptorDatabase.hpp"
#include "VulkanScene.hpp"
#include "../VulkanFunctions.hpp"
#include "../VulkanUtils.hpp"
#include <VulkanGenericStructures.h>
#include <unordered_map>
#include <cassert>
#include <algorithm>

namespace
{
	const std::array<uint32_t, (uint32_t)Vulkan::SceneDescriptorSetType::Count> SetCountsPerType =
	{
		1,                         //Texture set count
		1,                         //Material set count
		1,                         //Static data set count
		Utils::InFlightFrameCount, //Frame data set count
		Utils::InFlightFrameCount  //Frame + dynamic data set count
	};
}

Vulkan::SceneDescriptorDatabase::SceneDescriptorDatabase(VkDevice device): mDeviceRef(device)
{
	mDescriptorPool = VK_NULL_HANDLE;

	std::fill(mEntriesPerType.begin(), mEntriesPerType.end(), DatabaseEntry
	{
		.DescriptorSetLayout = VK_NULL_HANDLE,
		.AllocatedSetSpan    = Span<uint32_t>
		{
			.Begin = 0,
			.End   = 0
		},
		.ShaderStageFlags    = 0
	});
}

Vulkan::SceneDescriptorDatabase::~SceneDescriptorDatabase()
{
	ResetRegisteredSets();
}

Vulkan::SceneDescriptorSetType Vulkan::SceneDescriptorDatabase::ComputeSetType(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames)
{
	if(bindingNames.size() == 1 && bindingNames[0] == "SceneTextures")
	{
		//Texture set bindings
		assert(setBindings[0].binding         == 0);
		assert(setBindings[0].descriptorType  == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
		assert(setBindings[0].descriptorCount == (uint32_t)(-1));

		return SceneDescriptorSetType::TextureList;
	}
	else if(bindingNames.size() == 1 && bindingNames[0] == "SceneMaterials")
	{
		//Material set bindings
		assert(setBindings[0].binding         == 0);
		assert(setBindings[0].descriptorType  == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		assert(setBindings[0].descriptorCount == (uint32_t)(-1));

		return SceneDescriptorSetType::MaterialList;
	}
	else if(bindingNames.size() == 1 && bindingNames[0] == "SceneStaticObjectDatas")
	{
		//Static object data bindings
		assert(setBindings[0].binding         == 0);
		assert(setBindings[0].descriptorType  == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		assert(setBindings[0].descriptorCount == (uint32_t)(-1));

		return SceneDescriptorSetType::StaticObjectDataList;
	}
	else if(bindingNames.size() == 1 && bindingNames[0] == "SceneFrameData")
	{
		//Frame data bindings
		assert(setBindings[0].binding         == 0);
		assert(setBindings[0].descriptorType  == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		assert(setBindings[0].descriptorCount == 1);

		return SceneDescriptorSetType::FrameDataList;
	}
	else if(bindingNames.size() == 2 && bindingNames[0] == "SceneFrameData" && bindingNames[1] == "SceneDynamicObjectDatas")
	{
		//Frame data + dynamic object data
		assert(setBindings[0].binding         == 0);
		assert(setBindings[0].descriptorType  == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		assert(setBindings[0].descriptorCount == 1);

		assert(setBindings[1].binding         == 1);
		assert(setBindings[1].descriptorType  == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		assert(setBindings[1].descriptorCount == (uint32_t)(-1));

		return SceneDescriptorSetType::FrameAndDynamicObjectDataList;
	}

	return SceneDescriptorSetType::Unknown;
}

void Vulkan::SceneDescriptorDatabase::ResetRegisteredSets()
{
	SafeDestroyObject(vkDestroyDescriptorPool, mDeviceRef, mDescriptorPool);
	for(size_t entryIndex = 0; entryIndex < mEntriesPerType.size(); entryIndex++)
	{
		SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, mEntriesPerType[entryIndex].DescriptorSetLayout);
		mEntriesPerType[entryIndex].ShaderStageFlags = 0;
	}
}

void Vulkan::SceneDescriptorDatabase::RegisterRequiredSet(SceneDescriptorSetType setType, VkShaderStageFlags setShaderFlags)
{
	mEntriesPerType[(uint32_t)setType].ShaderStageFlags |= setShaderFlags;
}

void Vulkan::SceneDescriptorDatabase::RecreateDescriptorSets(const RenderableScene* sceneToCreateDescriptors)
{
	RecreateSetLayouts();
	AllocateSpaceForSets();
	UpdateDescriptorSets(sceneToCreateDescriptors);
}

void Vulkan::SceneDescriptorDatabase::UpdateDescriptorSets(const RenderableScene* sceneToCreateDescriptors)
{
	RecreateDescriptorPool(sceneToCreateDescriptors);
	AllocateSets(sceneToCreateDescriptors);
}

void Vulkan::SceneDescriptorDatabase::RecreateSetLayouts()
{
	std::array textureListBindingFlags               = {(VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};
	std::array materialListBindingFlags              = {(VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};
	std::array staticObjectDataBindingFlags          = {(VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};
	std::array frameDataBindingFlags                 = {(VkDescriptorBindingFlags)0};
	std::array frameAndDynamicObjectDataBindingFlags = {(VkDescriptorBindingFlags)0, (VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};

	std::array<VkDescriptorSetLayoutBindingFlagsCreateInfo, (size_t)SceneDescriptorSetType::Count> createFlagsPerType = std::to_array
	({
		//Textures set layout
		VkDescriptorSetLayoutBindingFlagsCreateInfo
		{
			.pNext         = nullptr,
			.bindingCount  = (uint32_t)textureListBindingFlags.size(),
			.pBindingFlags = textureListBindingFlags.data()
		},

		//Materials set layout
		VkDescriptorSetLayoutBindingFlagsCreateInfo
		{
			.pNext         = nullptr,
			.bindingCount  = (uint32_t)materialListBindingFlags.size(),
			.pBindingFlags = materialListBindingFlags.data()
		},

		//Static object datas set layout
		VkDescriptorSetLayoutBindingFlagsCreateInfo
		{
			.pNext         = nullptr,
			.bindingCount  = (uint32_t)staticObjectDataBindingFlags.size(),
			.pBindingFlags = staticObjectDataBindingFlags.data()
		},

		//Frame data set layout
		VkDescriptorSetLayoutBindingFlagsCreateInfo
		{
			.pNext         = nullptr,
			.bindingCount  = (uint32_t)frameDataBindingFlags.size(),
			.pBindingFlags = frameDataBindingFlags.data()
		},

		//Frame data + dynamic object datas set layout
		VkDescriptorSetLayoutBindingFlagsCreateInfo
		{
			.pNext         = nullptr,
			.bindingCount  = (uint32_t)frameAndDynamicObjectDataBindingFlags.size(),
			.pBindingFlags = frameAndDynamicObjectDataBindingFlags.data()
		},
	});

	std::array<vgs::GenericStructureChain<VkDescriptorSetLayoutCreateInfo>,  (size_t)SceneDescriptorSetType::Count> createInfosPerType;
	for(uint32_t typeIndex = 0; typeIndex < (uint32_t)SceneDescriptorSetType::Count; typeIndex++)
	{
		createInfosPerType[typeIndex].AppendToChain(createFlagsPerType[typeIndex]);

		const VkDescriptorSetLayoutCreateInfo& setLayoutCreateInfo = createInfosPerType[typeIndex].GetChainHead();
		ThrowIfFailed(vkCreateDescriptorSetLayout(mDeviceRef, &setLayoutCreateInfo, nullptr, &mEntriesPerType[typeIndex].DescriptorSetLayout));
	}
}

void Vulkan::SceneDescriptorDatabase::AllocateSpaceForSets()
{
	uint32_t setCount = 0;
	for(uint32_t typeIndex = 0; typeIndex < (uint32_t)(SceneDescriptorSetType::Count); typeIndex++)
	{
		if(mEntriesPerType[typeIndex].ShaderStageFlags != 0)
		{
			uint32_t currentSetCount = SetCountsPerType[typeIndex];
			mEntriesPerType[typeIndex].AllocatedSetSpan.Begin = setCount;
			mEntriesPerType[typeIndex].AllocatedSetSpan.End   = setCount + currentSetCount;
		}
	}

	mAllocatedSets.resize(setCount);
}

void Vulkan::SceneDescriptorDatabase::RecreateDescriptorPool(const RenderableScene* sceneToCreateDescriptors)
{
	VkDescriptorPoolSize textureDescriptorPoolSize = {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  .descriptorCount = 0};
	VkDescriptorPoolSize uniformDescriptorPoolSize = {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 0};

	std::array<uint32_t, (size_t)SceneDescriptorSetType::Count> descriptorCountsPerType = std::to_array
	({
		(uint32_t)sceneToCreateDescriptors->mSceneTextures.size(),                                                    //Texture descriptor count
		(uint32_t)(sceneToCreateDescriptors->mMaterialDataSize / sceneToCreateDescriptors->mMaterialChunkDataSize),   //Material descriptor count
		(uint32_t)(sceneToCreateDescriptors->mStaticObjectDataSize / sceneToCreateDescriptors->mObjectChunkDataSize), //Static data descriptor count
		Utils::InFlightFrameCount,                                                                                    //Frame data descriptor count
		Utils::InFlightFrameCount * (1 + (uint32_t)sceneToCreateDescriptors->mCurrFrameDataToUpdate.size())           //Frame + dynamic data descriptor count
	});

	std::array<VkDescriptorPoolSize*, (size_t)SceneDescriptorSetType::Count> poolSizesPerType = std::to_array
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
		if(mEntriesPerType[typeIndex].ShaderStageFlags != 0)
		{
			setCount += SetCountsPerType[typeIndex];
			poolSizesPerType[typeIndex]->descriptorCount += descriptorCountsPerType[typeIndex];
		}
	}

	std::array poolSizes = {textureDescriptorPoolSize, uniformDescriptorPoolSize};

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

void Vulkan::SceneDescriptorDatabase::AllocateSets(const RenderableScene* sceneToCreateDescriptors)
{
	std::array<uint32_t, (size_t)SceneDescriptorSetType::Count> variableCountsPerType = std::to_array
	({

		(uint32_t)sceneToCreateDescriptors->mSceneTextures.size(),                                                    //Textures set
		(uint32_t)(sceneToCreateDescriptors->mMaterialDataSize / sceneToCreateDescriptors->mMaterialChunkDataSize),   //Materials set
		(uint32_t)(sceneToCreateDescriptors->mStaticObjectDataSize / sceneToCreateDescriptors->mObjectChunkDataSize), //Static object datas set
		(uint32_t)0,                                                                                                  //Frame data set
		(uint32_t)sceneToCreateDescriptors->mCurrFrameDataToUpdate.size()                                             //Frame data + dynamic object datas set
	});

	std::vector<VkDescriptorSetLayout> setLayouts(mAllocatedSets.size());
	std::vector<uint32_t>              variableDescriptorCounts(mAllocatedSets.size());

	for(uint32_t typeIndex = 0; typeIndex < (uint32_t)(SceneDescriptorSetType::Count); typeIndex++)
	{
		Span<uint32_t> setSpan = mEntriesPerType[typeIndex].AllocatedSetSpan;
		for(uint32_t setIndex = setSpan.Begin; setIndex < setSpan.End; setIndex++)
		{
			setLayouts[setIndex]               = mEntriesPerType[typeIndex].DescriptorSetLayout;
			variableDescriptorCounts[setIndex] = variableCountsPerType[typeIndex];
		}
	}

	VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountAllocateInfo;
	variableCountAllocateInfo.pNext              = nullptr;
	variableCountAllocateInfo.descriptorSetCount = (uint32_t)variableDescriptorCounts.size();
	variableCountAllocateInfo.pDescriptorCounts  = variableDescriptorCounts.data();

	vgs::GenericStructureChain<VkDescriptorSetAllocateInfo> descriptorSetAllocateInfoChain;
	descriptorSetAllocateInfoChain.AppendToChain(variableCountAllocateInfo);

	VkDescriptorSetAllocateInfo setAllocateInfo = descriptorSetAllocateInfoChain.GetChainHead();
	setAllocateInfo.descriptorPool     = mDescriptorPool;
	setAllocateInfo.descriptorSetCount = (uint32_t)setLayouts.size();
	setAllocateInfo.pSetLayouts        = setLayouts.data();

	ThrowIfFailed(vkAllocateDescriptorSets(mDeviceRef, &setAllocateInfo, mAllocatedSets.data()));
}