#include "VulkanShaders.hpp"
#include "VulkanUtils.hpp"
#include "VulkanFunctions.hpp"
#include "../../Core/Util.hpp"
#include "../../Logging/Logger.hpp"
#include <VulkanGenericStructures.h>
#include <cassert>
#include <unordered_map>
#include <array>
#include <algorithm>

Vulkan::ShaderDatabase::ShaderDatabase(VkDevice device, SamplerManager* samplerManager, LoggerQueue* logger): mLogger(logger), mDeviceRef(device), mSamplerManagerRef(samplerManager)
{
	for(uint32_t bindingTypeIndex = 0; bindingTypeIndex < SharedDescriptorDatabase::TotalBindings; bindingTypeIndex++)
	{
		std::string_view bindingName = SharedDescriptorDatabase::DescriptorBindingNames[bindingTypeIndex];
		mSharedBindingTypes[bindingName] = (SharedDescriptorDatabase::SharedDescriptorBindingType)bindingTypeIndex;
	}
}

Vulkan::ShaderDatabase::~ShaderDatabase()
{
	//Destroy the non-flushed layouts
	for(VkDescriptorSetLayout setLayout: mSharedSetLayouts)
	{
		SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, setLayout);
	}
}

void Vulkan::ShaderDatabase::RegisterShaderGroup(std::string_view groupName, std::span<std::wstring> shaderPaths)
{
	assert(!mSetLayoutSpansPerShaderGroup.contains(groupName));

	for(const std::wstring& shaderPath: shaderPaths)
	{
		if(!mLoadedShaderModules.contains(shaderPath))
		{
			std::vector<uint32_t> shaderData;
			VulkanUtils::LoadShaderModuleFromFile(shaderPath, shaderData, mLogger);

			mLoadedShaderModules.emplace(std::make_pair(shaderPath, spv_reflect::ShaderModule(shaderData)));
		}
	}

	CollectBindings(shaderPaths);

	//std::vector<VkDescriptorSetLayoutBinding> setBindings;
	//std::vector<std::string>                  setBindingNames;
	//FindBindings(shaderPaths, setBindings, setBindingNames);

	//Span<uint32_t> layoutRecordSpan = 
	//{
	//	.Begin = (uint32_t)mSetLayoutRecords.size(),
	//	.End   = (uint32_t)(mSetLayoutRecords.size() + setSpans.size())
	//};

	//for(TypedSpan<uint32_t, VkShaderStageFlags> setSpan: setSpans)
	//{
	//	std::span<VkDescriptorSetLayoutBinding> bindingSpan = {setBindings.begin()     + setSpan.Begin, setBindings.begin()     + setSpan.End};
	//	std::span<std::string>                  nameSpan    = {setBindingNames.begin() + setSpan.Begin, setBindingNames.begin() + setSpan.End};

	//	uint16_t setDatabaseType   = 0xff; //Try shared set first
	//	uint16_t setRegisterResult = TryRegisterSet(bindingSpan, nameSpan);
	//	if(setRegisterResult == 0xff)
	//	{
	//		setRegisterResult = passDescriptorDatabaseBuilder->TryRegisterSet(bindingSpan, nameSpan);
	//		setDatabaseType   = passDescriptorDatabaseBuilder->GetPassTypeId();
	//	}

	//	mSetLayoutRecords.push_back(SetLayoutRecord
	//	{
	//		.Type = setDatabaseType,
	//		.Id   = setRegisterResult
	//	});
	//}

	//mSetLayoutSpansPerShaderGroup[groupName] = layoutRecordSpan;
}

void Vulkan::ShaderDatabase::GetRegisteredShaderInfo(const std::wstring& path, const uint32_t** outShaderData, uint32_t* outShaderSize) const
{
	assert(outShaderData != nullptr);
	assert(outShaderSize != nullptr);

	const spv_reflect::ShaderModule& shaderModule = mLoadedShaderModules.at(path);

	*outShaderData = shaderModule.GetCode();
	*outShaderSize = shaderModule.GetCodeSize();
}

void Vulkan::ShaderDatabase::GetPushConstantInfo(std::string_view groupName, std::string_view pushConstantName, uint32_t* outPushConstantOffset, VkShaderStageFlags* outShaderStages)
{
	Span<uint32_t> pushConstantSpan = mPushConstantSpansPerShaderGroup.at(groupName);

	const auto rangeBegin = mPushConstantRecords.begin() + pushConstantSpan.Begin;
	const auto rangeEnd   = mPushConstantRecords.begin() + pushConstantSpan.End;
	auto pushConstantRecord = std::find(rangeBegin, rangeEnd, [pushConstantName](const PushConstantRecord& rec)
	{
		return pushConstantName == rec.Name;
	});

	if(pushConstantRecord != rangeEnd)
	{
		if(outPushConstantOffset != nullptr)
		{
			*outPushConstantOffset = pushConstantRecord->Offset;
		}

		if(outShaderStages != nullptr)
		{
			*outShaderStages = pushConstantRecord->ShaderStages;
		}
	}
}

void Vulkan::ShaderDatabase::CollectBindings(const std::span<std::wstring> shaderModuleNames)
{
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
	std::vector<Span<uint32_t>>               bindingSpansPerSet;

	//Gather all bindings from all stages
	for(const std::wstring& shaderModuleName: shaderModuleNames)
	{
		const spv_reflect::ShaderModule& shaderModule = mLoadedShaderModules.at(shaderModuleName);

		uint32_t moduleSetCount = 0;
		shaderModule.EnumerateDescriptorSets(&moduleSetCount, nullptr);

		std::vector<SpvReflectDescriptorSet*> moduleSets(moduleSetCount);
		shaderModule.EnumerateDescriptorSets(&moduleSetCount, moduleSets.data());

		//Sort the sets array so existing sets are in the beginning and new ones are in the end
		Span<uint32_t> existingSetSpan = {.Begin = 0, .End = 0};
		Span<uint32_t> newSetSpan      = {.Begin = (uint32_t)moduleSets.size(), .End = (uint32_t)moduleSets.size()};
		for(uint32_t setIndex = 0; setIndex < moduleSets.size(); setIndex++)
		{
			SpvReflectDescriptorSet* moduleSet = moduleSets[setIndex];
			if(moduleSet->set < bindingSpansPerSet.size())
			{
				if(setIndex > existingSetSpan.End)
				{
					std::swap(moduleSets[existingSetSpan.End + 1], moduleSets[setIndex]);
				}

				existingSetSpan.End++;
			}
		}

		newSetSpan.Begin = existingSetSpan.End;


		uint32_t firstSetIndexToUpdate = (uint32_t)setLayoutBindings.size();
		for(uint32_t setIndex = existingSetSpan.Begin; setIndex < existingSetSpan.End; setIndex++)
		{
			SpvReflectDescriptorSet* moduleSet = moduleSets[setIndex];

			//Need to update existing set info
			uint32_t recordedSetSize = bindingSpansPerSet[moduleSet->set].End - bindingSpansPerSet[moduleSet->set].Begin;
			if(recordedSetSize < moduleSet->binding_count)
			{
				firstSetIndexToUpdate = std::min(firstSetIndexToUpdate, moduleSet->set);
			}
		}

		if(firstSetIndexToUpdate == bindingSpansPerSet.size() - 1)
		{
			//If the first existing set to update is also the last one, there's only one existing set to update
			SpvReflectDescriptorSet* moduleSet = moduleSets[existingSetSpan.Begin];

			uint32_t recordedSetSize = bindingSpansPerSet[moduleSet->set].End - bindingSpansPerSet[moduleSet->set].Begin;
			uint32_t sizeDiff        = moduleSet->binding_count - recordedSetSize;

			//Just append the data for the last set to the end, only the last set is changed
			setLayoutBindings.resize(setLayoutBindings.size() + sizeDiff);
			bindingSpansPerSet.back().End = bindingSpansPerSet.back().End + sizeDiff;
		}
		else if(firstSetIndexToUpdate < bindingSpansPerSet.size())
		{
			//Recreate the set binding data
			std::vector<VkDescriptorSetLayoutBinding> newSetLayoutBindings;
			newSetLayoutBindings.reserve(setLayoutBindings.size());

			//Copy the bindings that don't need to be re-sorted
			uint32_t preservedBindingCount = bindingSpansPerSet[firstSetIndexToUpdate].Begin;
			newSetLayoutBindings.resize(preservedBindingCount);
			memcpy(newSetLayoutBindings.data(), setLayoutBindings.data(), preservedBindingCount);

			//Recalculate new set sizes. To do it without allocating any new arrays, first shift all changed sets forward
			for(uint32_t setIndex = existingSetSpan.Begin; setIndex < existingSetSpan.End; setIndex++)
			{
				SpvReflectDescriptorSet* moduleSet = moduleSets[setIndex];
				if(moduleSet->set >= firstSetIndexToUpdate)
				{
					uint32_t recordedSetSize = bindingSpansPerSet[moduleSet->set].End - bindingSpansPerSet[moduleSet->set].Begin;

					uint32_t sizeDiff = 0;	
					if(recordedSetSize < moduleSet->binding_count)
					{
						sizeDiff = moduleSet->binding_count - recordedSetSize;
					}

					bindingSpansPerSet[moduleSet->set].Begin += sizeDiff;
					bindingSpansPerSet[moduleSet->set].End   += sizeDiff;
				}
			}

			//Next, copy the bindings
			uint32_t spanBegin = preservedBindingCount;
			for(uint32_t setIndex = firstSetIndexToUpdate; setIndex < (uint32_t)bindingSpansPerSet.size(); setIndex++)
			{
				Span<uint32_t> originalSpan = {.Begin = spanBegin, .End = ???};
				uint32_t originalSpanSize = originalSpan.End - originalSpan.Begin;

				Span<uint32_t> newSpan = ???;
				memcpy(newSetLayoutBindings.data() + newSpan.Begin, setLayoutBindings.data() + originalSpan.Begin, originalSpanSize);

				bindingSpansPerSet[setIndex] = newSpan;
			}
		}

		////Now outSetSpans[i].End contains the binding count for ith set. Accumulate the spans
		//uint32_t accumulatedBindingCount = 0;
		//for(TypedSpan<uint32_t, VkShaderStageFlags>& bindingSpan: outBindingSpans)
		//{
		//	uint32_t writtenBindingCount = bindingSpan.End;

		//	bindingSpan.Begin = accumulatedBindingCount;
		//	bindingSpan.End   = accumulatedBindingCount + writtenBindingCount;

		//	accumulatedBindingCount += writtenBindingCount;
		//}
	}
}

void Vulkan::ShaderDatabase::FindBindings(const std::span<std::wstring> shaderModuleNames, std::vector<VkDescriptorSetLayoutBinding>& outSetBindings, std::vector<std::string>& outBindingNames) const
{	
	//outSetBindings.clear();
	//outBindingNames.clear();

	//std::vector<SpvReflectDescriptorBinding*> allBindings;
	//CollectBindings(shaderModuleNames, allBindings);

	//uint32_t totalSetCount = 0;
	//for(SpvReflectDescriptorBinding* binding: allBindings)
	//{
	//	totalSetCount = std::max(binding->set + 1, totalSetCount);
	//}

	//std::sort(allBindings.begin(), allBindings.end(), [](SpvReflectDescriptorBinding* left, SpvReflectDescriptorBinding* right)
	//{
	//	return strcmp(left->name, right->name) < 0;
	//});

	////Merge bindings by name
	//std::vector<SpvReflectDescriptorBinding*> uniqueBindings;
	//uniqueBindings.reserve(allBindings.size());

	//const char* currBindingName = "";
	//for(SpvReflectDescriptorBinding* binding: allBindings)
	//{
	//	if(strcmp(binding->name, currBindingName) == 0)
	//	{
	//		#error "Not implemented"
	//	}
	//	else
	//	{
	//		#error "Not implemented"
	//	}
	//}

	////Sort by <set, binding>
	//std::sort(uniqueBindings.begin(), uniqueBindings.end(), [](SpvReflectDescriptorBinding* left, SpvReflectDescriptorBinding* right)
	//{
	//	return left->binding < right->binding;
	//});

	//std::stable_sort(uniqueBindings.begin(), uniqueBindings.end(), [](SpvReflectDescriptorBinding* left, SpvReflectDescriptorBinding* right)
	//{
	//	return left->set < right->set;
	//});

	////Calculate the binding count for each set and store it in outSetSpans[binding->set].End
	//outBindingSpans.resize(totalSetCount, {.Begin = 0, .End = 0, .Type = 0});
	//for(size_t spanIndex = 0; spanIndex < moduleBindingSpans.size(); spanIndex++)
	//{
	//	TypedSpan<uint32_t, SpvReflectShaderStageFlagBits> bindingSpan = moduleBindingSpans[spanIndex];
	//	for(uint32_t bindingIndex = bindingSpan.Begin; bindingIndex < bindingSpan.End; bindingIndex++)
	//	{
	//		SpvReflectDescriptorBinding* binding = allBindings[bindingIndex];
	//		outBindingSpans[binding->set].End   = std::max(outBindingSpans[binding->set].End, binding->binding + 1);
	//		outBindingSpans[binding->set].Type |= SpvToVkShaderStage(bindingSpan.Type);
	//	}
	//}

	////Now outSetSpans[i].End contains the binding count for ith set. Accumulate the spans
	//uint32_t accumulatedBindingCount = 0;
	//for(TypedSpan<uint32_t, VkShaderStageFlags>& bindingSpan: outBindingSpans)
	//{
	//	uint32_t writtenBindingCount = bindingSpan.End;

	//	bindingSpan.Begin = accumulatedBindingCount;
	//	bindingSpan.End   = accumulatedBindingCount + writtenBindingCount;

	//	accumulatedBindingCount += writtenBindingCount;
	//}

	////Fill out the bindings
	//outSetBindings.resize(accumulatedBindingCount);
	//outBindingNames.resize(accumulatedBindingCount);
	//for(TypedSpan<uint32_t, SpvReflectShaderStageFlagBits> moduleSpan: moduleBindingSpans)
	//{
	//	const VkShaderStageFlags                      shaderStage       = SpvToVkShaderStage(moduleSpan.Type);
	//	const std::span<SpvReflectDescriptorBinding*> moduleBindingSpan = {allBindings.begin() + moduleSpan.Begin, allBindings.begin() + moduleSpan.End};

	//	for(SpvReflectDescriptorBinding* spvDescriptorBinding: moduleBindingSpan)
	//	{
	//		TypedSpan<uint32_t, VkShaderStageFlags> setSpan          = outBindingSpans[spvDescriptorBinding->set];
	//		uint32_t                                flatBindingIndex = setSpan.Begin + spvDescriptorBinding->binding;

	//		//Name should be defined in every shader for every uniform block
	//		assert(spvDescriptorBinding->name != nullptr && spvDescriptorBinding->name[0] != '\0');

	//		if(outSetBindings[flatBindingIndex].binding == (uint32_t)(-1))
	//		{
	//			outBindings[flatBindingIndex]          = spvDescriptorBinding;
	//			outBindingStageFlags[flatBindingIndex] = shaderStage;
	//		}
	//		else
	//		{
	//			//Make sure the binding is the same
	//			SpvReflectDescriptorBinding* definedBinding = outBindings[flatBindingIndex];

	//			assert(strcmp(spvDescriptorBinding->name, definedBinding->name) == 0);
	//			assert(spvDescriptorBinding->resource_type == definedBinding->resource_type);
	//			assert(spvDescriptorBinding->count         == definedBinding->count);

	//			outBindingStageFlags[flatBindingIndex] |= shaderStage;
	//		}

	//		VkShaderStageFlags shaderStageFlags = bindingStageFlags[bindingIndex];

	//		uint32_t descriptorCount = spvDescriptorBinding->count;
	//		if(spvDescriptorBinding->type_description->op == SpvOpTypeRuntimeArray)
	//		{
	//			descriptorCount = (uint32_t)(-1); //A way to check for variable descriptor count
	//		}

	//		VkDescriptorSetLayoutBinding vkDescriptorBinding =
	//		{
	//			.binding            = spvDescriptorBinding->binding,
	//			.descriptorType     = SpvToVkDescriptorType(spvDescriptorBinding->descriptor_type),
	//			.descriptorCount    = descriptorCount,
	//			.stageFlags         = shaderStageFlags,
	//			.pImmutableSamplers = nullptr
	//		};
	//	
	//		outSetBindings.push_back(vkDescriptorBinding);
	//		outBindingNames.push_back(setBinding->name);
	//	}
	//}
}

uint16_t Vulkan::ShaderDatabase::TryRegisterSet(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames)
{
	//Assume empty set is invalid
	if(bindingNames.size() == 0)
	{
		return 0xff;
	}

	uint32_t matchingLayoutIndex = (uint32_t)(-1);
	for(size_t layoutIndex = 0; layoutIndex < mSetBindingSpansPerLayout.size(); layoutIndex++)
	{
		Span<uint32_t> layoutBindingSpan = mSetBindingSpansPerLayout[layoutIndex];

		uint32_t layoutBindingCount = layoutBindingSpan.Begin - layoutBindingSpan.End;
		if(layoutBindingCount == (uint32_t)setBindings.size())
		{
			//Try to match the set
			uint32_t layoutBindingIndex = 0;
			while(layoutBindingIndex < layoutBindingCount)
			{
				auto bindingTypeIt = mBindingTypes.find(bindingNames[layoutBindingIndex]);
				if(bindingTypeIt == mBindingTypes.end())
				{
					//All binding names should match
					return 0xff;
				}

				if(mSetLayoutBindingTypesFlat[(size_t)layoutBindingSpan.Begin + layoutBindingIndex] != bindingTypeIt->second)
				{
					//Binding types don't match
					break;
				}
			}

			if(layoutBindingIndex == layoutBindingCount)
			{
				//All binding types matched
				matchingLayoutIndex = layoutIndex;
				break;
			}
		}
	}
	
	if(matchingLayoutIndex == (uint32_t)-1)
	{
		//Allocate the space for bindings if needed
		size_t registeredBindingCount = mSetLayoutBindingsFlat.size();
		size_t addedBindingCount      = setBindings.size();

		Span<uint32_t> newLayoutSpan = 
		{
			.Begin = (uint32_t)registeredBindingCount,
			.End   = (uint32_t)(registeredBindingCount + addedBindingCount)
		};

		mSetBindingSpansPerLayout.push_back(newLayoutSpan);
		mSetLayoutBindingsFlat.resize(registeredBindingCount + addedBindingCount);

		//Validate bindings
		for(uint32_t bindingIndex = newLayoutSpan.Begin; bindingIndex < newLayoutSpan.End; bindingIndex++)
		{
			uint32_t layoutBindingIndex = bindingIndex - newLayoutSpan.Begin;
	
			const VkDescriptorSetLayoutBinding&                   bindingInfo = setBindings[layoutBindingIndex];
			SharedDescriptorDatabase::SharedDescriptorBindingType bindingType = mBindingTypes.at(bindingNames[layoutBindingIndex]);

			assert(ValidateNewBinding(bindingInfo, bindingType));
			assert(bindingInfo.descriptorCount != (uint32_t)(-1) || bindingInfo.binding == (setBindings.size() - 1)); //Only the last binding can be variable sized
			assert(bindingInfo.binding == layoutBindingIndex);

			mSetLayoutBindingTypesFlat[layoutBindingIndex] = bindingType;
			mSetLayoutBindingsFlat[layoutBindingIndex]     = bindingInfo;

			if(bindingType == SharedDescriptorDatabase::SharedDescriptorBindingType::SamplerList)
			{
				mSetLayoutBindingsFlat[layoutBindingIndex].pImmutableSamplers = mSamplerManagerRef->GetSamplerVariableArray();
			}
		}

		matchingLayoutIndex = (uint32_t)(mSetBindingSpansPerLayout.size() - 1);
	}
	else
	{
		Span<uint32_t> layoutBindingSpan = mSetBindingSpansPerLayout[matchingLayoutIndex];

		VkShaderStageFlags setStageFlags;
		for(uint32_t bindingIndex = 0; bindingIndex < setBindings.size(); bindingIndex++)
		{
			const VkDescriptorSetLayoutBinding& newBindingInfo = setBindings[bindingIndex];
			VkDescriptorSetLayoutBinding& existingBindingInfo  = mSetLayoutBindingsFlat[(size_t)layoutBindingSpan.Begin + bindingIndex];

			assert(ValidateExistingBinding(newBindingInfo, existingBindingInfo));
			assert(newBindingInfo.binding == bindingIndex);

			existingBindingInfo.stageFlags |= newBindingInfo.stageFlags;
		}
	}

	return (uint32_t)(matchingLayoutIndex);
}

void Vulkan::ShaderDatabase::BuildSetLayouts()
{
	mSetLayouts.resize(mSetBindingSpansPerLayout.size());

	size_t layoutCount       = mSetBindingSpansPerLayout.size();
	size_t totalBindingCount = mSetLayoutBindingsFlat.size();

	std::vector<VkDescriptorBindingFlags> bindingFlagsPerBinding(totalBindingCount);
	for(size_t layoutIndex = 0; layoutIndex < layoutCount; layoutIndex++)
	{
		Span<uint32_t> layoutBindingSpan  = mSetBindingSpansPerLayout[layoutIndex];
		uint32_t       layoutBindingCount = layoutBindingSpan.End - layoutBindingSpan.Begin;

		for(uint32_t bindingIndex = layoutBindingSpan.Begin; bindingIndex < layoutBindingSpan.End; bindingIndex++)
		{
			uint32_t bindingTypeIndex = (uint32_t)mSetLayoutBindingTypesFlat[bindingIndex];
			bindingFlagsPerBinding[bindingIndex] = DescriptorFlagsPerBinding[bindingTypeIndex];
		}

		VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo =
		{
			.pNext         = nullptr,
			.bindingCount  = layoutBindingCount,
			.pBindingFlags = bindingFlagsPerBinding.data() + layoutBindingSpan.Begin
		};

		vgs::GenericStructureChain<VkDescriptorSetLayoutCreateInfo> setLayoutCreateInfoChain;
		setLayoutCreateInfoChain.AppendToChain(bindingFlagsCreateInfo);

		VkDescriptorSetLayoutCreateInfo& setLayoutCreateInfo = setLayoutCreateInfoChain.GetChainHead();
		setLayoutCreateInfo.flags        = 0;
		setLayoutCreateInfo.pBindings    = mSetLayoutBindingsFlat.data() + layoutBindingSpan.Begin;
		setLayoutCreateInfo.bindingCount = layoutBindingCount;

		ThrowIfFailed(vkCreateDescriptorSetLayout(mDeviceRef, &setLayoutCreateInfo, nullptr, &mSetLayouts[layoutIndex]));	
	}
}

void Vulkan::ShaderDatabase::FlushSetLayoutInfos(SharedDescriptorDatabase* databaseToBuild)
{
	size_t layoutCount = mSetBindingSpansPerLayout.size();

	//Move these to the scene directly
	std::swap(databaseToBuild->mSetFormatsPerLayout, mSetBindingSpansPerLayout);
	std::swap(databaseToBuild->mSetFormatsFlat,      mSetLayoutBindingTypesFlat);

	std::swap(databaseToBuild->mSetLayouts, mSetLayouts);

	size_t totalSetCount = 0;
	databaseToBuild->mSetsPerLayout.resize(layoutCount);
	for(size_t layoutIndex = 0; layoutIndex < databaseToBuild->mSetFormatsPerLayout.size(); layoutIndex++)
	{
		Span<uint32_t> layoutBindingSpan  = databaseToBuild->mSetFormatsPerLayout[layoutIndex];
		uint32_t       layoutBindingCount = layoutBindingSpan.End - layoutBindingSpan.Begin;

		size_t layoutSetCount = 1;
		for(uint32_t bindingIndex = layoutBindingSpan.Begin; bindingIndex < layoutBindingSpan.End; bindingIndex++)
		{
			uint32_t layoutBindingIndex = bindingIndex - layoutBindingSpan.Begin;
			layoutSetCount = std::lcm(layoutSetCount, SharedDescriptorDatabase::FrameCountsPerBinding[layoutBindingIndex]);
		}

		databaseToBuild->mSetsPerLayout[layoutIndex] = 		
		{
			.Begin = (uint32_t)totalSetCount,
			.End   = (uint32_t)(totalSetCount + layoutSetCount)
		};

		totalSetCount += layoutSetCount;
	}

	databaseToBuild->mSets.resize(totalSetCount);
}

bool Vulkan::ShaderDatabase::ValidateNewBinding(const VkDescriptorSetLayoutBinding& bindingInfo, SharedDescriptorDatabase::SharedDescriptorBindingType bindingType) const
{
	uint32_t bindingTypeIndex = (uint32_t)(bindingType);
	if(bindingInfo.descriptorType != SharedDescriptorDatabase::DescriptorTypesPerBinding[bindingTypeIndex])
	{
		return false;
	}

	if(bindingInfo.descriptorCount == (uint32_t)(-1) && DescriptorFlagsPerBinding[bindingTypeIndex] != VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT)
	{
		return false;
	}

	return true;
}

bool Vulkan::ShaderDatabase::ValidateExistingBinding(const VkDescriptorSetLayoutBinding& newBindingInfo, const VkDescriptorSetLayoutBinding& existingBindingInfo) const
{
	if(newBindingInfo.descriptorType != existingBindingInfo.descriptorType)
	{
		return false;
	}

	if(newBindingInfo.descriptorCount != existingBindingInfo.descriptorCount)
	{
		return false;
	}

	if(newBindingInfo.binding != existingBindingInfo.binding)
	{
		return false;
	}

	return true;
}

VkShaderStageFlagBits Vulkan::ShaderDatabase::SpvToVkShaderStage(SpvReflectShaderStageFlagBits spvShaderStage) const
{
	switch (spvShaderStage)
	{
	case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
		return VK_SHADER_STAGE_VERTEX_BIT;

	case SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
		return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

	case SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
		return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

	case SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT:
		return VK_SHADER_STAGE_GEOMETRY_BIT;

	case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
		return VK_SHADER_STAGE_FRAGMENT_BIT;

	case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
		return VK_SHADER_STAGE_COMPUTE_BIT;

	case SPV_REFLECT_SHADER_STAGE_TASK_BIT_NV:
		return VK_SHADER_STAGE_TASK_BIT_NV;

	case SPV_REFLECT_SHADER_STAGE_MESH_BIT_NV:
		return VK_SHADER_STAGE_MESH_BIT_NV;

	case SPV_REFLECT_SHADER_STAGE_RAYGEN_BIT_KHR:
		return VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	case SPV_REFLECT_SHADER_STAGE_ANY_HIT_BIT_KHR:
		return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

	case SPV_REFLECT_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
		return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

	case SPV_REFLECT_SHADER_STAGE_MISS_BIT_KHR:
		return VK_SHADER_STAGE_MISS_BIT_KHR;

	case SPV_REFLECT_SHADER_STAGE_INTERSECTION_BIT_KHR:
		return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

	case SPV_REFLECT_SHADER_STAGE_CALLABLE_BIT_KHR:
		return VK_SHADER_STAGE_CALLABLE_BIT_KHR;

	default:
		return VK_SHADER_STAGE_ALL;
	}
}

VkDescriptorType Vulkan::ShaderDatabase::SpvToVkDescriptorType(SpvReflectDescriptorType spvDescriptorType) const
{
	switch (spvDescriptorType)
	{
	case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
		return VK_DESCRIPTOR_TYPE_SAMPLER;

	case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

	case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

	case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;

	case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;

	case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

	case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;

	case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;

	case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
		return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;

	case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
		return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

	default:
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	}
}