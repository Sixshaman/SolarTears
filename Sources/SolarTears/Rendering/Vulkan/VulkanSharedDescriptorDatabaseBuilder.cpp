#include "VulkanSharedDescriptorDatabaseBuilder.hpp"
#include "VulkanFunctions.hpp"
#include "VulkanUtils.hpp"
#include <VulkanGenericStructures.h>

Vulkan::SharedDescriptorDatabaseBuilder::SharedDescriptorDatabaseBuilder(SharedDescriptorDatabase* databaseToBuild): mDatabaseToBuild(databaseToBuild)
{
	std::fill(mShaderFlagsPerSceneType.begin(), mShaderFlagsPerSceneType.end(), VkShaderStageFlags(0));
}

Vulkan::SharedDescriptorDatabaseBuilder::~SharedDescriptorDatabaseBuilder()
{
}

Vulkan::SetRegisterResult Vulkan::SharedDescriptorDatabaseBuilder::TryRegisterSet(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames)
{
	SetRegisterResult sceneRegisterResult = TryRegisterSceneSet(setBindings, bindingNames);
	if(sceneRegisterResult == SetRegisterResult::UndefinedSharedSet)
	{
		return TryRegisterSamplerSet(setBindings, bindingNames);
	}

	return sceneRegisterResult;
}

void Vulkan::SharedDescriptorDatabaseBuilder::Build()
{
	SafeDestroyObject(vkDestroyDescriptorSetLayout, mDatabaseToBuild->mDeviceRef, mDatabaseToBuild->mSamplerDescriptorSetLayout);
	for (size_t entryIndex = 0; entryIndex < mDatabaseToBuild->mSceneSetLayouts.size(); entryIndex++)
	{
		SafeDestroyObject(vkDestroyDescriptorSetLayout, mDatabaseToBuild->mDeviceRef, mDatabaseToBuild->mSceneSetLayouts[entryIndex]);
	}

	RecreateSamplerSetLayouts();
	RecreateSceneSetLayouts();
}

Vulkan::SetRegisterResult Vulkan::SharedDescriptorDatabaseBuilder::TryRegisterSamplerSet(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames)
{
	//Try to validate as a sampler set
	if(bindingNames.size() == 1 && bindingNames[0] == "Samplers")
	{
		//Sampler set bindings
		if(setBindings[0].descriptorType != VK_DESCRIPTOR_TYPE_SAMPLER || setBindings[0].binding != 0 || setBindings[0].descriptorCount != (uint32_t)(-1))
		{
			return SetRegisterResult::ValidateError;
		}
		
		mSamplerShaderFlags |= setBindings[0].stageFlags;
		return SetRegisterResult::Success;
	}

	return SetRegisterResult::UndefinedSharedSet;
}

Vulkan::SetRegisterResult Vulkan::SharedDescriptorDatabaseBuilder::TryRegisterSceneSet(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames)
{
	constexpr std::array<uint32_t, SharedDescriptorDatabase::TotalSceneSetLayouts> sceneSetBindingCounts = std::to_array
	({
		1u, //1 binding for textures set
		1u, //1 binding for materials set
		1u, //1 binding for static object data set
		1u, //1 binding for frame data set
		2u  //2 bindings for frame + dynamic object data set
	});

	constexpr uint32_t TotalBindings = std::accumulate(sceneSetBindingCounts.begin(), sceneSetBindingCounts.end(), 0);
	constexpr std::array<std::string_view, TotalBindings> setBindingNames = std::to_array(
	{
		std::string_view("SceneTextures"),
		std::string_view("SceneMaterials"),
		std::string_view("SceneStaticObjectDatas"),
		std::string_view("SceneFrameData"),
		std::string_view("SceneFrameData"), std::string_view("SceneDynamicObjectDatas")
	});

	constexpr std::array<VkDescriptorType, TotalBindings> setBindingTypes = std::to_array(
	{
		VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
	});

	constexpr std::array<uint32_t, TotalBindings> setDescriptorCounts = std::to_array(
	{
		(uint32_t)(-1),
		(uint32_t)(-1),
		(uint32_t)(-1),
		1u,
		1u, (uint32_t)(-1)
	});

	for(uint32_t typeIndex = 0, cumulativeBindingIndex = 0; typeIndex < SharedDescriptorDatabase::TotalSceneSetLayouts; typeIndex++)
	{
		const uint32_t bindingCount = sceneSetBindingCounts[typeIndex];
		if(bindingNames.size() != bindingCount)
		{
			continue;
		}

		bool allBindingsMatch    = true;
		bool allBindingsValidate = true;
		VkShaderStageFlags bindingStageFlags = 0;
		for(uint32_t bindingIndex = 0; bindingIndex < bindingCount; bindingIndex++, cumulativeBindingIndex++)
		{
			if(bindingNames[bindingIndex] == setBindingNames[cumulativeBindingIndex])
			{
				const VkDescriptorSetLayoutBinding& setBinding = setBindings[bindingIndex];
				allBindingsValidate &= (setBinding.binding         == bindingIndex);
				allBindingsValidate &= (setBinding.descriptorType  == setBindingTypes[cumulativeBindingIndex]);
				allBindingsValidate &= (setBinding.descriptorCount == setDescriptorCounts[cumulativeBindingIndex]);

				bindingStageFlags |= setBinding.stageFlags;
			}
			else
			{
				allBindingsMatch = false;
				break;
			}
		}

		if(allBindingsMatch && allBindingsValidate)
		{
			mShaderFlagsPerSceneType[typeIndex] |= bindingStageFlags;
			return SetRegisterResult::Success;
		}
		else if(allBindingsMatch)
		{
			return SetRegisterResult::ValidateError;
		}
	}

	return SetRegisterResult::UndefinedSharedSet;
}

void Vulkan::SharedDescriptorDatabaseBuilder::RecreateSamplerSetLayouts()
{
	SafeDestroyObject(vkDestroyDescriptorSetLayout, mDatabaseToBuild->mDeviceRef, mDatabaseToBuild->mSamplerDescriptorSetLayout);

	VkDescriptorSetLayoutBinding samplerBinding;
	samplerBinding.binding            = 0;
	samplerBinding.descriptorCount    = (uint32_t)mDatabaseToBuild->mSamplers.size();
	samplerBinding.descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER;
	samplerBinding.stageFlags         = mSamplerShaderFlags;
	samplerBinding.pImmutableSamplers = mDatabaseToBuild->mSamplers.data();

	std::array setLayoutBindings = {samplerBinding};

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
	descriptorSetLayoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext        = nullptr;
	descriptorSetLayoutCreateInfo.flags        = 0;
	descriptorSetLayoutCreateInfo.bindingCount = (uint32_t)setLayoutBindings.size();
	descriptorSetLayoutCreateInfo.pBindings    = setLayoutBindings.data();

	ThrowIfFailed(vkCreateDescriptorSetLayout(mDatabaseToBuild->mDeviceRef, &descriptorSetLayoutCreateInfo, nullptr, &mDatabaseToBuild->mSamplerDescriptorSetLayout));
}

void Vulkan::SharedDescriptorDatabaseBuilder::RecreateSceneSetLayouts()
{
	std::array textureListBindingFlags               = {(VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};
	std::array materialListBindingFlags              = {(VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};
	std::array staticObjectDataBindingFlags          = {(VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};
	std::array frameDataBindingFlags                 = {(VkDescriptorBindingFlags)0};
	std::array frameAndDynamicObjectDataBindingFlags = {(VkDescriptorBindingFlags)0, (VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT};

	std::array<VkDescriptorSetLayoutBindingFlagsCreateInfo, (size_t)SharedDescriptorDatabase::TotalSceneSetLayouts> createFlagsPerType = std::to_array
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

	std::array<vgs::GenericStructureChain<VkDescriptorSetLayoutCreateInfo>,  (size_t)SharedDescriptorDatabase::TotalSceneSetLayouts> createInfosPerType;
	for(uint32_t typeIndex = 0; typeIndex < SharedDescriptorDatabase::TotalSceneSetLayouts; typeIndex++)
	{
		SafeDestroyObject(vkDestroyDescriptorSetLayout, mDatabaseToBuild->mDeviceRef, mDatabaseToBuild->mSceneSetLayouts[typeIndex]);
		createInfosPerType[typeIndex].AppendToChain(createFlagsPerType[typeIndex]);

		const VkDescriptorSetLayoutCreateInfo& setLayoutCreateInfo = createInfosPerType[typeIndex].GetChainHead();
		ThrowIfFailed(vkCreateDescriptorSetLayout(mDatabaseToBuild->mDeviceRef, &setLayoutCreateInfo, nullptr, &mDatabaseToBuild->mSceneSetLayouts[typeIndex]));
	}
}