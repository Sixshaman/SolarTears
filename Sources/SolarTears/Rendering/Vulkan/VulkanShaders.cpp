#include "VulkanShaders.hpp"
#include "VulkanUtils.hpp"
#include "VulkanFunctions.hpp"
#include "VulkanSamplers.hpp"
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


void Vulkan::ShaderDatabase::FlushSharedSetLayoutInfos(SharedDescriptorDatabase* databaseToBuild)
{
	size_t layoutCount = mSharedSetBindingSpansPerLayout.size();

	//Move these to the scene directly
	std::swap(databaseToBuild->mSetFormatsPerLayout, mSharedSetBindingSpansPerLayout);
	std::swap(databaseToBuild->mSetFormatsFlat,      mSharedSetLayoutBindingTypesFlat);

	std::swap(databaseToBuild->mSetLayouts, mSharedSetLayouts);

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

void Vulkan::ShaderDatabase::CollectBindings(const std::span<std::wstring> shaderModuleNames)
{
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
	std::vector<std::string>                  setLayoutBindingNames;
	std::vector<Span<uint32_t>>               bindingSpansPerSet;

	//Gather all bindings from all stages
	for(const std::wstring& shaderModuleName: shaderModuleNames)
	{
		const spv_reflect::ShaderModule& shaderModule = mLoadedShaderModules.at(shaderModuleName);

		uint32_t moduleSetCount = 0;
		shaderModule.EnumerateDescriptorSets(&moduleSetCount, nullptr);

		std::vector<SpvReflectDescriptorSet*> moduleSets(moduleSetCount);
		shaderModule.EnumerateDescriptorSets(&moduleSetCount, moduleSets.data());

		std::span<SpvReflectDescriptorSet*> existingSets;
		std::span<SpvReflectDescriptorSet*> newSets;
		SplitNewAndExistingSets((uint32_t)bindingSpansPerSet.size(), moduleSets, &existingSets, &newSets);

		std::vector<std::pair<SpvReflectDescriptorSet*, uint32_t>> existingSetsWithSizes;
		CalculateExistingSetSizePairs(existingSets, bindingSpansPerSet, existingSetsWithSizes);

		std::vector<std::pair<SpvReflectDescriptorSet*, uint32_t>> newSetsWithSizes;
		CalculateNewSetSizePairs(newSets, newSetsWithSizes);

		VkShaderStageFlags shaderStage = SpvToVkShaderStage(shaderModule.GetShaderStage());
		UpdateExistingSets(existingSetsWithSizes, setLayoutBindings, bindingSpansPerSet, shaderStage);
		AddNewSets(newSetsWithSizes, setLayoutBindings, bindingSpansPerSet, shaderStage);

		 
	}
}

void Vulkan::ShaderDatabase::SplitNewAndExistingSets(uint32_t existingSetCount, std::vector<SpvReflectDescriptorSet*>& inoutSpvSets, std::span<SpvReflectDescriptorSet*>* outExistingSetSpan, std::span<SpvReflectDescriptorSet*>* outNewSetSpan)
{
	assert(outExistingSetSpan != nullptr);
	assert(outNewSetSpan != nullptr);
		
	uint32_t moduleSetCount = (uint32_t)inoutSpvSets.size();

	uint32_t splitPos = 0;
	for(uint32_t setIndex = 0; setIndex < moduleSetCount; setIndex++)
	{
		SpvReflectDescriptorSet* moduleSet = inoutSpvSets[setIndex];
		if(moduleSet->set < existingSetCount)
		{
			if(setIndex > splitPos)
			{
				std::swap(inoutSpvSets[splitPos + 1], inoutSpvSets[setIndex]);
			}

			splitPos++;
		}
	}

	*outExistingSetSpan = std::span(inoutSpvSets.begin(), inoutSpvSets.begin() + splitPos);
	*outNewSetSpan      = std::span(inoutSpvSets.begin() + splitPos, inoutSpvSets.end());
}

void Vulkan::ShaderDatabase::CalculateExistingSetSizePairs(const std::span<SpvReflectDescriptorSet*> moduleExistingSetSpan, std::vector<Span<uint32_t>> existingBindingSpansPerSet, std::vector<std::pair<SpvReflectDescriptorSet*, uint32_t>>& outExistingSetsWithSizes)
{
	outExistingSetsWithSizes.reserve(moduleExistingSetSpan.size());
	for(SpvReflectDescriptorSet* moduleSet: moduleExistingSetSpan)
	{
		uint32_t setSize = existingBindingSpansPerSet[moduleSet->set].End - existingBindingSpansPerSet[moduleSet->set].Begin;
		for(uint32_t bindingIndex = 0; bindingIndex < moduleSet->binding_count; bindingIndex++)
		{
			setSize = std::max(setSize, moduleSet->bindings[bindingIndex]->binding + 1);
		}

		outExistingSetsWithSizes.push_back(std::make_pair(moduleSet, setSize));
	}
}

void Vulkan::ShaderDatabase::CalculateNewSetSizePairs(const std::span<SpvReflectDescriptorSet*> moduleNewSetSpan, std::vector<std::pair<SpvReflectDescriptorSet*, uint32_t>>& outNewSetsWithSizes)
{
	outNewSetsWithSizes.reserve(moduleNewSetSpan.size());
	for(SpvReflectDescriptorSet* moduleSet: moduleNewSetSpan)
	{
		uint32_t setSize = 0;
		for(uint32_t bindingIndex = 0; bindingIndex < moduleSet->binding_count; bindingIndex++)
		{
			setSize = std::max(setSize, moduleSet->bindings[bindingIndex]->binding + 1);
		}

		outNewSetsWithSizes.push_back(std::make_pair(moduleSet, setSize));
	}
}

void Vulkan::ShaderDatabase::UpdateExistingSets(const std::vector<std::pair<SpvReflectDescriptorSet*, uint32_t>> setUpdates, std::vector<VkDescriptorSetLayoutBinding>& inoutBindings, std::vector<Span<uint32_t>>& inoutSetSpans, VkShaderStageFlags stageFlags)
{
	uint32_t firstSetIndexToUpdate = (uint32_t)inoutSetSpans.size();
	for(size_t moduleSetIndex = 0; moduleSetIndex < setUpdates.size(); moduleSetIndex++)
	{
		SpvReflectDescriptorSet* moduleSet     = setUpdates[moduleSetIndex].first;
		uint32_t                 moduleSetSize = setUpdates[moduleSetIndex].second;

		//Need to update existing set info
		uint32_t recordedSetSize = inoutSetSpans[moduleSet->set].End - inoutSetSpans[moduleSet->set].Begin;
		if(recordedSetSize < moduleSetSize)
		{
			firstSetIndexToUpdate = std::min(firstSetIndexToUpdate, moduleSet->set);
		}
	}

	if(firstSetIndexToUpdate == inoutSetSpans.size() - 1)
	{
		//If the first existing set to update is also the last one, there's only one existing set to update
		SpvReflectDescriptorSet* moduleSet     = setUpdates[0].first;
		uint32_t                 moduleSetSize = setUpdates[0].second;

		uint32_t recordedSetSize = inoutSetSpans.back().End - inoutSetSpans.back().Begin;
		uint32_t sizeDiff        = moduleSetSize - recordedSetSize;

		//Just append the data for the last set to the end, only the last set is changed
		inoutBindings.resize(inoutBindings.size() + sizeDiff, VkDescriptorSetLayoutBinding{.stageFlags = 0}); //Uninitialized bindings are marked with stageFlags == 0
		inoutSetSpans.back().End = inoutSetSpans.back().End + sizeDiff;
	}
	else if(firstSetIndexToUpdate < inoutSetSpans.size())
	{
		//Recreate the set binding data
		std::vector<VkDescriptorSetLayoutBinding> newSetLayoutBindings;
		newSetLayoutBindings.reserve(inoutBindings.size());

		//Copy the bindings that don't need to be re-sorted
		uint32_t preservedBindingCount = inoutSetSpans[firstSetIndexToUpdate].Begin;
		newSetLayoutBindings.resize(preservedBindingCount);
		memcpy(newSetLayoutBindings.data(), inoutBindings.data(), preservedBindingCount);

		//Recalculate new set sizes. To do it in-place, first shift the beginning points of spans left while leaving end points untouched
		for(auto setUpdate: setUpdates)
		{
			SpvReflectDescriptorSet* moduleSet = setUpdate.first;
			if(moduleSet->set >= firstSetIndexToUpdate)
			{
				uint32_t recordedSetSize = inoutSetSpans[moduleSet->set].End - inoutSetSpans[moduleSet->set].Begin;

				uint32_t sizeDiff = 0;	
				if(recordedSetSize < setUpdate.second)
				{
					sizeDiff = setUpdate.second - recordedSetSize;
					inoutSetSpans[moduleSet->set].Begin -= sizeDiff;
				}
			}
		}

		//Copy the existing bindings
		uint32_t originalSpanBegin = preservedBindingCount;
		for(uint32_t setIndex = firstSetIndexToUpdate; setIndex < (uint32_t)inoutSetSpans.size(); setIndex++)
		{
			//Restore the original span from the original end point
			Span<uint32_t> originalSpan     = {.Begin = originalSpanBegin, .End = inoutSetSpans[setIndex].End};
			uint32_t       originalSpanSize = originalSpan.End - originalSpan.Begin;

			originalSpanBegin = inoutSetSpans[setIndex].End; //Shift to the next span

			//Calculate the real new span (shifted to newSpanBegin)
			uint32_t       newSpanSize = inoutSetSpans[setIndex].End - inoutSetSpans[setIndex].Begin; //Safe even with unsigned wrap
			Span<uint32_t> newSpan     = {.Begin = (uint32_t)newSetLayoutBindings.size(), .End = (uint32_t)newSetLayoutBindings.size() + newSpanSize};
			inoutSetSpans[setIndex] = newSpan;

			newSetLayoutBindings.resize(newSetLayoutBindings.size() + newSpanSize, VkDescriptorSetLayoutBinding{.stageFlags = 0});
			memcpy(newSetLayoutBindings.data() + newSpan.Begin, inoutBindings.data() + originalSpan.Begin, originalSpanSize);
		}

		inoutBindings.swap(newSetLayoutBindings);
	}

	for(auto moduleSetPair: setUpdates)
	{
		SpvReflectDescriptorSet* moduleSet = moduleSetPair.first;

		size_t bindingStart = inoutSetSpans[moduleSet->set].Begin;
		for(uint32_t moduleBindingIndex = 0; moduleBindingIndex < moduleSet->binding_count; moduleBindingIndex++)
		{
			SpvReflectDescriptorBinding*  moduleBinding   = moduleSet->bindings[moduleBindingIndex];
			VkDescriptorSetLayoutBinding* bindingToUpdate = &inoutBindings[bindingStart + moduleBinding->binding];

			if(bindingToUpdate->stageFlags == 0) //Uninitialized binding
			{
				InitializeDescriptorSetBinding(moduleBinding, bindingToUpdate, stageFlags);
			}
			else //Existing binding
			{
				UpdateValidateDescriptorSetBinding(moduleBinding, bindingToUpdate, stageFlags);
			}
		}
	}
}

void Vulkan::ShaderDatabase::AddNewSets(const std::vector<std::pair<SpvReflectDescriptorSet*, uint32_t>> newSets, std::vector<VkDescriptorSetLayoutBinding>& inoutBindings, std::vector<Span<uint32_t>>& inoutSetSpans, VkShaderStageFlags stageFlags)
{
	uint32_t oldBindingCount = (uint32_t)inoutBindings.size();
	uint32_t newBindingCount = std::accumulate(newSets.begin(), newSets.end(), oldBindingCount);
	inoutBindings.resize(newBindingCount, VkDescriptorSetLayoutBinding{.stageFlags = 0});

	uint32_t oldSetCount = (uint32_t)inoutSetSpans.size();
	uint32_t newSetCount = oldSetCount;
	for(auto newSet: newSets)
	{
		newSetCount = std::max(newSetCount, newSet.first->set + 1);
	}

	inoutSetSpans.resize(newSetCount, Span<uint32_t>{.Begin = 0, .End = 0});
	for(auto newSet: newSets)
	{
		SpvReflectDescriptorSet* moduleSet     = newSet.first;
		uint32_t                 moduleSetSize = newSet.second;

		Span<uint32_t> bindingSpan = Span<uint32_t>
		{
			.Begin = oldBindingCount,
			.End   = oldBindingCount + moduleSetSize
		};

		inoutSetSpans[moduleSet->set] = bindingSpan;
	}

	uint32_t spanOffset = oldBindingCount;
	for(uint32_t setIndex = oldSetCount; setIndex < newSetCount; setIndex++)
	{
		uint32_t setSize = inoutSetSpans[setIndex].End - inoutSetSpans[setIndex].Begin;

		inoutSetSpans[setIndex].Begin = spanOffset;
		inoutSetSpans[setIndex].End   = spanOffset + setSize;

		spanOffset = inoutSetSpans[setIndex].End;
	}

	for(auto newSet: newSets)
	{
		SpvReflectDescriptorSet* moduleSet = newSet.first;

		size_t bindingStart = inoutSetSpans[moduleSet->set].Begin;
		for(uint32_t moduleBindingIndex = 0; moduleBindingIndex < moduleSet->binding_count; moduleBindingIndex++)
		{
			SpvReflectDescriptorBinding* moduleBinding = moduleSet->bindings[moduleBindingIndex];
			InitializeDescriptorSetBinding(moduleBinding, &inoutBindings[bindingStart + moduleBinding->binding], stageFlags);
		}
	}
}

void Vulkan::ShaderDatabase::InitializeDescriptorSetBinding(SpvReflectDescriptorBinding* moduleBinding, VkDescriptorSetLayoutBinding* bindingToInitialize, VkShaderStageFlags shaderStage)
{
	assert(bindingToInitialize != nullptr);

	bindingToInitialize->binding            = moduleBinding->binding;
	bindingToInitialize->descriptorType     = SpvToVkDescriptorType(moduleBinding->descriptor_type);
	bindingToInitialize->stageFlags         = shaderStage;
	bindingToInitialize->pImmutableSamplers = nullptr;

	if(moduleBinding->type_description->op == SpvOpTypeRuntimeArray)
	{
		bindingToInitialize->descriptorCount = (uint32_t)(-1); //Mark the variable descriptor count
	}
	else
	{
		bindingToInitialize->descriptorCount = moduleBinding->count;
	}
}

void Vulkan::ShaderDatabase::UpdateValidateDescriptorSetBinding(SpvReflectDescriptorBinding* moduleBinding, VkDescriptorSetLayoutBinding* bindingToUpdate, VkShaderStageFlags shaderStage)
{
	assert(bindingToUpdate != nullptr);

	assert(bindingToUpdate->binding        == moduleBinding->binding);
	assert(bindingToUpdate->descriptorType == SpvToVkDescriptorType(moduleBinding->descriptor_type));

	if(moduleBinding->type_description->op == SpvOpTypeRuntimeArray)
	{
		assert(bindingToUpdate->descriptorCount == (uint32_t)(-1));
	}
	else
	{
		assert(bindingToUpdate->descriptorCount == moduleBinding->count);
	}

	bindingToUpdate->stageFlags |= shaderStage;
}

uint16_t Vulkan::ShaderDatabase::TryRegisterSet(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames)
{
	//Assume empty set is invalid
	if(bindingNames.size() == 0)
	{
		return 0xff;
	}

	uint32_t matchingLayoutIndex = (uint32_t)(-1);
	for(size_t layoutIndex = 0; layoutIndex < mSharedSetBindingSpansPerLayout.size(); layoutIndex++)
	{
		Span<uint32_t> layoutBindingSpan = mSharedSetBindingSpansPerLayout[layoutIndex];

		uint32_t layoutBindingCount = layoutBindingSpan.Begin - layoutBindingSpan.End;
		if(layoutBindingCount == (uint32_t)setBindings.size())
		{
			//Try to match the set
			uint32_t layoutBindingIndex = 0;
			while(layoutBindingIndex < layoutBindingCount)
			{
				auto bindingTypeIt = mSharedBindingTypes.find(bindingNames[layoutBindingIndex]);
				if(bindingTypeIt == mSharedBindingTypes.end())
				{
					//All binding names should match
					return 0xff;
				}

				if(mSharedSetLayoutBindingTypesFlat[(size_t)layoutBindingSpan.Begin + layoutBindingIndex] != bindingTypeIt->second)
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
		size_t registeredBindingCount = mSharedSetLayoutBindingsFlat.size();
		size_t addedBindingCount      = setBindings.size();

		Span<uint32_t> newLayoutSpan = 
		{
			.Begin = (uint32_t)registeredBindingCount,
			.End   = (uint32_t)(registeredBindingCount + addedBindingCount)
		};

		mSharedSetBindingSpansPerLayout.push_back(newLayoutSpan);
		mSharedSetLayoutBindingsFlat.resize(registeredBindingCount + addedBindingCount);

		//Validate bindings
		for(uint32_t bindingIndex = newLayoutSpan.Begin; bindingIndex < newLayoutSpan.End; bindingIndex++)
		{
			uint32_t layoutBindingIndex = bindingIndex - newLayoutSpan.Begin;
	
			const VkDescriptorSetLayoutBinding&                   bindingInfo = setBindings[layoutBindingIndex];
			SharedDescriptorDatabase::SharedDescriptorBindingType bindingType = mSharedBindingTypes.at(bindingNames[layoutBindingIndex]);

			assert(ValidateNewBinding(bindingInfo, bindingType));
			assert(bindingInfo.descriptorCount != (uint32_t)(-1) || bindingInfo.binding == (setBindings.size() - 1)); //Only the last binding can be variable sized
			assert(bindingInfo.binding == layoutBindingIndex);

			mSharedSetLayoutBindingTypesFlat[layoutBindingIndex] = bindingType;
			mSharedSetLayoutBindingsFlat[layoutBindingIndex]     = bindingInfo;

			if(bindingType == SharedDescriptorDatabase::SharedDescriptorBindingType::SamplerList)
			{
				mSharedSetLayoutBindingsFlat[layoutBindingIndex].pImmutableSamplers = mSamplerManagerRef->GetSamplerVariableArray();
			}
		}

		matchingLayoutIndex = (uint32_t)(mSharedSetBindingSpansPerLayout.size() - 1);
	}
	else
	{
		Span<uint32_t> layoutBindingSpan = mSharedSetBindingSpansPerLayout[matchingLayoutIndex];

		VkShaderStageFlags setStageFlags;
		for(uint32_t bindingIndex = 0; bindingIndex < setBindings.size(); bindingIndex++)
		{
			const VkDescriptorSetLayoutBinding& newBindingInfo = setBindings[bindingIndex];
			VkDescriptorSetLayoutBinding& existingBindingInfo  = mSharedSetLayoutBindingsFlat[(size_t)layoutBindingSpan.Begin + bindingIndex];

			assert(ValidateExistingBinding(newBindingInfo, existingBindingInfo));
			assert(newBindingInfo.binding == bindingIndex);

			existingBindingInfo.stageFlags |= newBindingInfo.stageFlags;
		}
	}

	return (uint32_t)(matchingLayoutIndex);
}

void Vulkan::ShaderDatabase::BuildSetLayouts()
{
	mSharedSetLayouts.resize(mSharedSetBindingSpansPerLayout.size());

	size_t layoutCount       = mSharedSetBindingSpansPerLayout.size();
	size_t totalBindingCount = mSharedSetLayoutBindingsFlat.size();

	std::vector<VkDescriptorBindingFlags> bindingFlagsPerBinding(totalBindingCount);
	for(size_t layoutIndex = 0; layoutIndex < layoutCount; layoutIndex++)
	{
		Span<uint32_t> layoutBindingSpan  = mSharedSetBindingSpansPerLayout[layoutIndex];
		uint32_t       layoutBindingCount = layoutBindingSpan.End - layoutBindingSpan.Begin;

		for(uint32_t bindingIndex = layoutBindingSpan.Begin; bindingIndex < layoutBindingSpan.End; bindingIndex++)
		{
			uint32_t bindingTypeIndex = (uint32_t)mSharedSetLayoutBindingTypesFlat[bindingIndex];
			bindingFlagsPerBinding[bindingIndex] = SharedDescriptorDatabase::DescriptorFlagsPerBinding[bindingTypeIndex];
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
		setLayoutCreateInfo.pBindings    = mSharedSetLayoutBindingsFlat.data() + layoutBindingSpan.Begin;
		setLayoutCreateInfo.bindingCount = layoutBindingCount;

		ThrowIfFailed(vkCreateDescriptorSetLayout(mDeviceRef, &setLayoutCreateInfo, nullptr, &mSharedSetLayouts[layoutIndex]));
	}
}

bool Vulkan::ShaderDatabase::ValidateNewBinding(const VkDescriptorSetLayoutBinding& bindingInfo, SharedDescriptorDatabase::SharedDescriptorBindingType bindingType) const
{
	uint32_t bindingTypeIndex = (uint32_t)(bindingType);
	if(bindingInfo.descriptorType != SharedDescriptorDatabase::DescriptorTypesPerBinding[bindingTypeIndex])
	{
		return false;
	}

	if(bindingInfo.descriptorCount == (uint32_t)(-1) && SharedDescriptorDatabase::DescriptorFlagsPerBinding[bindingTypeIndex] != VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT)
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