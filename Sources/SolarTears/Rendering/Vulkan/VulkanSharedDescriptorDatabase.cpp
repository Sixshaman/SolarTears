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
	mSamplerShaderFlags         = 0;

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
	SetRegisterResult sceneRegisterResult = TryRegisterSceneSet(setBindings, bindingNames);
	if(sceneRegisterResult == SetRegisterResult::UndefinedSharedSet)
	{
		return TryRegisterSamplerSet(setBindings, bindingNames);
	}

	return sceneRegisterResult;
}

void Vulkan::SharedDescriptorDatabase::ResetSetsLayouts()
{
	mSamplerShaderFlags = 0;

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

Vulkan::SetRegisterResult Vulkan::SharedDescriptorDatabase::TryRegisterSamplerSet(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames)
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

Vulkan::SetRegisterResult Vulkan::SharedDescriptorDatabase::TryRegisterSceneSet(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames)
{
	constexpr std::array<uint32_t, TotalSceneSetLayouts> sceneSetBindingCounts = std::to_array
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

	for(uint32_t typeIndex = 0, cumulativeBindingIndex = 0; typeIndex < TotalSceneSetLayouts; typeIndex++)
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
			mSceneEntriesPerType[typeIndex].ShaderStageFlags |= bindingStageFlags;
			return SetRegisterResult::Success;
		}
		else if(allBindingsMatch)
		{
			return SetRegisterResult::ValidateError;
		}
	}

	return SetRegisterResult::UndefinedSharedSet;
}

void Vulkan::SharedDescriptorDatabase::RecreateSamplerSetLayouts()
{
	SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, mSamplerDescriptorSetLayout);

	VkDescriptorSetLayoutBinding samplerBinding;
	samplerBinding.binding            = 0;
	samplerBinding.descriptorCount    = (uint32_t)mSamplers.size();
	samplerBinding.descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER;
	samplerBinding.stageFlags         = mSamplerShaderFlags;
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
		if(mSceneEntriesPerType[typeIndex].ShaderStageFlags != 0)
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

void Vulkan::SharedDescriptorDatabase::AllocateSets(const RenderableScene* sceneToCreateDescriptors)
{
	//std::array<uint32_t, (size_t)SceneDescriptorSetType::Count> variableCountsPerType = std::to_array
	//({
	//	(uint32_t)sceneToCreateDescriptors->mSceneTextures.size(),                                                    //Textures set
	//	(uint32_t)(sceneToCreateDescriptors->mMaterialDataSize / sceneToCreateDescriptors->mMaterialChunkDataSize),   //Materials set
	//	(uint32_t)(sceneToCreateDescriptors->mStaticObjectDataSize / sceneToCreateDescriptors->mObjectChunkDataSize), //Static object datas set
	//	(uint32_t)0,                                                                                                  //Frame data set
	//	(uint32_t)sceneToCreateDescriptors->mCurrFrameDataToUpdate.size()                                             //Frame data + dynamic object datas set
	//});

	//for(uint32_t typeIndex = 0; typeIndex < (uint32_t)(SceneDescriptorSetType::Count); typeIndex++)
	//{
	//	if(mSceneEntriesPerType[typeIndex].DescriptorSetLayout != VK_NULL_HANDLE)
	//	{
	//		uint32_t currentSetCount = SetCountsPerType[typeIndex];
	//		mEntriesPerType[typeIndex].AllocatedSetSpan.Begin = setCount;
	//		mEntriesPerType[typeIndex].AllocatedSetSpan.End   = setCount + currentSetCount;
	//	}

	//	Span<uint32_t> setSpan = mEntriesPerType[typeIndex].AllocatedSetSpan;
	//	for(uint32_t setIndex = setSpan.Begin; setIndex < setSpan.End; setIndex++)
	//	{
	//		setLayouts[setIndex]               = mEntriesPerType[typeIndex].DescriptorSetLayout;
	//		variableDescriptorCounts[setIndex] = variableCountsPerType[typeIndex];
	//	}
	//}

	//VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountAllocateInfo;
	//variableCountAllocateInfo.pNext              = nullptr;
	//variableCountAllocateInfo.descriptorSetCount = (uint32_t)variableDescriptorCounts.size();
	//variableCountAllocateInfo.pDescriptorCounts  = variableDescriptorCounts.data();

	//vgs::GenericStructureChain<VkDescriptorSetAllocateInfo> descriptorSetAllocateInfoChain;
	//descriptorSetAllocateInfoChain.AppendToChain(variableCountAllocateInfo);

	//VkDescriptorSetAllocateInfo setAllocateInfo = descriptorSetAllocateInfoChain.GetChainHead();
	//setAllocateInfo.descriptorPool     = mDescriptorPool;
	//setAllocateInfo.descriptorSetCount = (uint32_t)setLayouts.size();
	//setAllocateInfo.pSetLayouts        = setLayouts.data();

	//ThrowIfFailed(vkAllocateDescriptorSets(mDeviceRef, &setAllocateInfo, mAllocatedSets.data()));
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