#include "VulkanSharedDescriptorDatabase.hpp"
#include "VulkanUtils.hpp"
#include "VulkanFunctions.hpp"
#include <VulkanGenericStructures.h>

Vulkan::SharedDescriptorDatabase::SharedDescriptorDatabase(const VkDevice device): mDeviceRef(device)
{
	mDescriptorPool = VK_NULL_HANDLE;

	mSamplerDescriptorSetLayout   = VK_NULL_HANDLE;
	mSamplerDescriptorSet         = VK_NULL_HANDLE;
	mRegisteredSamplerShaderFlags = 0;

	std::fill(mSceneEntriesPerType.begin(), mSceneEntriesPerType.end(), SceneSetDatabaseEntry
	{
		.DescriptorSetLayout = VK_NULL_HANDLE,
		.DescriptorSetSpan   = {mSceneSets.begin(), mSceneSets.end()},
		.ShaderStageFlags    = 0
	});
	
	std::fill(mSceneSets.begin(), mSceneSets.end(), VK_NULL_HANDLE);

	ResetSetsLayouts();
}

Vulkan::SharedDescriptorDatabase::~SharedDescriptorDatabase()
{
	SafeDestroyObject(vkDestroyDescriptorPool, mDeviceRef, mDescriptorPool);

	ResetSetsLayouts();
	for(size_t i = 0; i < mSamplers.size(); i++)
	{
		SafeDestroyObject(vkDestroySampler, mDeviceRef, mSamplers[i]);
	}
}

Vulkan::SetRegisterResult Vulkan::SharedDescriptorDatabase::TryRegisterSet(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames)
{
	constexpr uint32_t TotalSamplerSets = 1;
	constexpr std::array<uint32_t, TotalSceneSetLayouts + TotalSamplerSets> setBindingCounts =
	{
		1, //1 binding for sampler set
		1, //1 binding for textures set
		1, //1 binding for materials set
		1, //1 binding for static object data set
		1, //1 binding for frame data set
		2  //2 bindings for frame + dynamic object data set
	};

	constexpr uint32_t TotalBindings = std::accumulate(setBindingCounts.begin(), setBindingCounts.end(), 0);
	constexpr std::array<std::string_view, TotalBindings> setBindingNames =
	{
		"Samplers",
		"SceneTextures",
		"SceneMaterials",
		"SceneStaticObjectDatas",
		"SceneFrameData",
		"SceneFrameData", "SceneDynamicObjectDatas"
	};



	//if(bindingNames.size() == 1 && bindingNames[0] == "Samplers")
	//{
	//	//Sampler set bindings
	//	if(setBindings[0].descriptorType != VK_DESCRIPTOR_TYPE_SAMPLER || setBindings[0].binding != 0 || setBindings[0].descriptorCount != (uint32_t)(-1))
	//	{
	//		return SetRegisterResult::ValidateError;
	//	}
	//	
	//	mSamplerShaderFlags |= setBindings[0].stageFlags;
	//	return SetRegisterResult::Success;
	//}
	//else if(bindingNames.size() == 1 && bindingNames[0] == "SceneTextures")
	//{
	//	//Texture set bindings
	//	if(setBindings[0].descriptorType != VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || setBindings[0].binding != 0 || setBindings[0].descriptorCount != (uint32_t)(-1))
	//	{
	//		return SetRegisterResult::ValidateError;
	//	}

	//	mSceneEntriesPerType[(uint32_t)SceneDescriptorSetType::TextureList].ShaderStageFlags |= setBindings[0].stageFlags;
	//	return SetRegisterResult::Success;
	//}
	//else if(bindingNames.size() == 1 && bindingNames[0] == "SceneMaterials")
	//{
	//	//Material set bindings
	//	assert(setBindings[0].binding         == 0);
	//	assert(setBindings[0].descriptorType  == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	//	assert(setBindings[0].descriptorCount == (uint32_t)(-1));

	//	return SceneDescriptorSetType::MaterialList;
	//}
	//else if(bindingNames.size() == 1 && bindingNames[0] == "SceneStaticObjectDatas")
	//{
	//	//Static object data bindings
	//	assert(setBindings[0].binding         == 0);
	//	assert(setBindings[0].descriptorType  == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	//	assert(setBindings[0].descriptorCount == (uint32_t)(-1));

	//	return SceneDescriptorSetType::StaticObjectDataList;
	//}
	//else if(bindingNames.size() == 1 && bindingNames[0] == "SceneFrameData")
	//{
	//	//Frame data bindings
	//	assert(setBindings[0].binding         == 0);
	//	assert(setBindings[0].descriptorType  == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	//	assert(setBindings[0].descriptorCount == 1);

	//	return SceneDescriptorSetType::FrameDataList;
	//}
	//else if(bindingNames.size() == 2 && bindingNames[0] == "SceneFrameData" && bindingNames[1] == "SceneDynamicObjectDatas")
	//{
	//	//Frame data + dynamic object data
	//	assert(setBindings[0].binding         == 0);
	//	assert(setBindings[0].descriptorType  == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	//	assert(setBindings[0].descriptorCount == 1);

	//	assert(setBindings[1].binding         == 1);
	//	assert(setBindings[1].descriptorType  == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	//	assert(setBindings[1].descriptorCount == (uint32_t)(-1));

	//	return SceneDescriptorSetType::FrameAndDynamicObjectDataList;
	//}

	return SetRegisterResult::UndefinedSharedSet;
}

void Vulkan::SharedDescriptorDatabase::ResetSetsLayouts()
{
	mRegisteredSamplerShaderFlags = 0;

	SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, mSamplerDescriptorSetLayout);
	for(size_t entryIndex = 0; entryIndex < mSceneEntriesPerType.size(); entryIndex++)
	{
		SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, mSceneEntriesPerType[entryIndex].DescriptorSetLayout);
		mSceneEntriesPerType[entryIndex].ShaderStageFlags = 0;
	}
}

void Vulkan::SharedDescriptorDatabase::RecreateSetLayouts()
{
	RecreateSamplerSetLayouts();
	RecreateSceneSetLayouts();
}

void Vulkan::SharedDescriptorDatabase::RecreateSamplerSetLayouts()
{
	SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, mSamplerDescriptorSetLayout);

	VkDescriptorSetLayoutBinding samplerBinding;
	samplerBinding.binding            = 0;
	samplerBinding.descriptorCount    = (uint32_t)mSamplers.size();
	samplerBinding.descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER;
	samplerBinding.stageFlags         = mRegisteredSamplerShaderFlags;
	samplerBinding.pImmutableSamplers = mSamplers.data();

	std::array setLayoutBindings = {samplerBinding};

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
	descriptorSetLayoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext        = nullptr;
	descriptorSetLayoutCreateInfo.flags        = 0;
	descriptorSetLayoutCreateInfo.bindingCount = (uint32_t)setLayoutBindings.size();
	descriptorSetLayoutCreateInfo.pBindings    = setLayoutBindings.data();

	ThrowIfFailed(vkCreateDescriptorSetLayout(mDeviceRef, &descriptorSetLayoutCreateInfo, nullptr, &mSamplerDescriptorSetLayout));
}

void Vulkan::SharedDescriptorDatabase::RecreateSceneSetLayouts()
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
		SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, mSceneEntriesPerType[typeIndex].DescriptorSetLayout);
		createInfosPerType[typeIndex].AppendToChain(createFlagsPerType[typeIndex]);

		const VkDescriptorSetLayoutCreateInfo& setLayoutCreateInfo = createInfosPerType[typeIndex].GetChainHead();
		ThrowIfFailed(vkCreateDescriptorSetLayout(mDeviceRef, &setLayoutCreateInfo, nullptr, &mSceneEntriesPerType[typeIndex].DescriptorSetLayout));
	}
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