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
	//Initialize shared binding records
	RegisterPass(RenderPassType::None);
	for(uint16_t bindingTypeIndex = 0; bindingTypeIndex < SharedDescriptorDatabase::TotalBindings; bindingTypeIndex++)
	{
		RegisterPassBinding(RenderPassType::None, SharedDescriptorDatabase::DescriptorBindingNames[bindingTypeIndex], SharedDescriptorDatabase::DescriptorTypesPerBinding[bindingTypeIndex]);

		auto it = mBindingRecordMap.find(SharedDescriptorDatabase::DescriptorBindingNames[bindingTypeIndex]);
		it->second.DescriptorFlags = SharedDescriptorDatabase::DescriptorFlagsPerBinding[bindingTypeIndex];
		it->second.FrameCount      = SharedDescriptorDatabase::FrameCountsPerBinding[bindingTypeIndex];
	}
}

Vulkan::ShaderDatabase::~ShaderDatabase()
{
	//Destroy the non-flushed layouts
	for(SetLayoutRecordNode setLayoutNode: mSetLayoutNodes)
	{
		SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, setLayoutNode.SetLayout);
	}
}

void Vulkan::ShaderDatabase::RegisterPass(RenderPassType passType)
{
	assert(!mDomainRecordMap.contains(passType));

	mDomainRecordMap[passType] = DomainRecord
	{
		.Domain                 = (uint16_t)mLayoutHeadNodeIndicesPerDomain.size(),
		.RegisteredBindingCount = 0
	};

	mLayoutHeadNodeIndicesPerDomain.push_back((uint32_t)-1);
}

void Vulkan::ShaderDatabase::RegisterPassBinding(RenderPassType passType, std::string_view bindingName, VkDescriptorType descriptorType)
{
	DomainRecord& domainRecord = mDomainRecordMap.at(passType);
	mBindingRecordMap[bindingName] = BindingRecord
	{
		.Domain          = domainRecord.Domain,
		.Type            = (uint16_t)(domainRecord.RegisteredBindingCount + 1),
		.DescriptorType  = descriptorType,
		.DescriptorFlags = 0,
		.FrameCount      = 1
	};
}

void Vulkan::ShaderDatabase::SetPassBindingFlags(std::string_view bindingName, VkDescriptorBindingFlags descriptorFlags)
{
	auto it = mBindingRecordMap.find(bindingName);
	assert(it != mBindingRecordMap.end());

	it->second.DescriptorFlags = descriptorFlags;
}

void Vulkan::ShaderDatabase::SetPassBindingFrameCount(std::string_view bindingName, uint32_t frameCount)
{
	assert(frameCount != 0);

	auto it = mBindingRecordMap.find(bindingName);
	assert(it != mBindingRecordMap.end());

	it->second.FrameCount = frameCount;
}

void Vulkan::ShaderDatabase::RegisterShaderGroup(std::string_view groupName, std::span<std::wstring> shaderPaths)
{
	assert(!mSetLayoutIndexSpansPerShaderGroup.contains(groupName));

	for(const std::wstring& shaderPath: shaderPaths)
	{
		if(!mLoadedShaderModules.contains(shaderPath))
		{
			std::vector<uint32_t> shaderData;
			VulkanUtils::LoadShaderModuleFromFile(shaderPath, shaderData, mLogger);

			mLoadedShaderModules.emplace(std::make_pair(shaderPath, spv_reflect::ShaderModule(shaderData)));
		}
	}

	RegisterBindings(groupName, shaderPaths);
	RegisterPushConstants(groupName, shaderPaths);
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
	Span<uint32_t> pushConstantSpan = mPushConstantSpansPerShaderGroup.at(groupName).RecordSpan;

	const auto rangeBegin = mPushConstantRecords.begin() + pushConstantSpan.Begin;
	const auto rangeEnd   = mPushConstantRecords.begin() + pushConstantSpan.End;
	auto pushConstantRecord = std::lower_bound(rangeBegin, rangeEnd, [pushConstantName](const PushConstantRecord& rec)
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

void Vulkan::ShaderDatabase::FlushSharedSetLayouts(SharedDescriptorDatabase* databaseToBuild)
{
	databaseToBuild->mSetFormatsPerLayout.clear();
	databaseToBuild->mSetFormatsFlat.clear();
	databaseToBuild->mSetLayouts.clear();

	uint32_t totalSetCount = 0;
	uint32_t nodeIndex = mLayoutHeadNodeIndicesPerDomain[SharedSetDomain];
	while(nodeIndex != (uint32_t)(-1))
	{
		SetLayoutRecordNode setLayoutNode      = mSetLayoutNodes[nodeIndex];
		uint32_t            layoutBindingCount = setLayoutNode.BindingSpan.End - setLayoutNode.BindingSpan.Begin;

		uint32_t oldFormatCount = (uint32_t)databaseToBuild->mSetFormatsFlat.size();
		Span<uint32_t> formatSpan =
		{
			.Begin = oldFormatCount,
			.End   = oldFormatCount + layoutBindingCount
		};

		databaseToBuild->mSetFormatsPerLayout.push_back(formatSpan);
		databaseToBuild->mSetFormatsFlat.resize(databaseToBuild->mSetFormatsFlat.size() + layoutBindingCount);

		size_t layoutSetCount = 1;
		for(uint32_t bindingIndex = setLayoutNode.BindingSpan.Begin; bindingIndex < setLayoutNode.BindingSpan.End; bindingIndex++)
		{
			uint32_t layoutBindingIndex = bindingIndex - setLayoutNode.BindingSpan.Begin;
			layoutSetCount = std::lcm(layoutSetCount, mLayoutBindingFrameCountsFlat[layoutBindingIndex]);

			databaseToBuild->mSetFormatsFlat[formatSpan.Begin + layoutBindingIndex] = (SharedDescriptorDatabase::SharedDescriptorBindingType)mLayoutBindingTypesFlat[bindingIndex];
		}

		databaseToBuild->mSetsPerLayout.push_back(Span<uint32_t> 		
		{
			.Begin = (uint32_t)totalSetCount,
			.End   = (uint32_t)(totalSetCount + layoutSetCount)
		});

		totalSetCount += layoutSetCount;
	}

	databaseToBuild->mSets.resize(totalSetCount);
}

void Vulkan::ShaderDatabase::RegisterBindings(const std::string_view groupName, const std::span<std::wstring> shaderModuleNames)
{
	std::vector<Span<uint32_t>> bindingSpansPerSet;

	std::vector<SpvReflectDescriptorBinding*> spvSetLayoutBindings;
	std::vector<VkShaderStageFlags>           bindingStageFlags;

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
		SplitSetsByPivot((uint32_t)bindingSpansPerSet.size(), moduleSets, &existingSets, &newSets);

		MergeExistingSetBindings(existingSets, spvSetLayoutBindings, bindingSpansPerSet);
		MergeNewSetBindings(newSets, spvSetLayoutBindings, bindingSpansPerSet);

		VkShaderStageFlags shaderStage = SpvToVkShaderStage(shaderModule.GetShaderStage());
		bindingStageFlags.resize(spvSetLayoutBindings.size(), (VkShaderStageFlags)0);

		for(SpvReflectDescriptorSet* moduleSet: moduleSets)
		{
			Span<uint32_t> setSpan = bindingSpansPerSet[moduleSet->set];
			for(uint32_t bindingIndex = 0; bindingIndex < moduleSet->binding_count; bindingIndex++)
			{
				SpvReflectDescriptorBinding* binding = moduleSet->bindings[bindingIndex];
				bindingStageFlags[setSpan.Begin + binding->binding] |= shaderStage;
			}
		}
	}

	//Construct binding list and name list
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
	std::vector<std::string_view>             setLayoutBindingNames;
	for(size_t bindingIndex = 0; bindingIndex < spvSetLayoutBindings.size(); bindingIndex++)
	{
		SpvReflectDescriptorBinding* spvBinding = spvSetLayoutBindings[bindingIndex];
		assert(spvBinding != nullptr);

		VkDescriptorSetLayoutBinding setLayoutBinding;
		setLayoutBinding.binding            = spvBinding->binding,
		setLayoutBinding.descriptorType     = SpvToVkDescriptorType(spvBinding->descriptor_type),
		setLayoutBinding.descriptorCount    = spvBinding->count,
		setLayoutBinding.stageFlags         = bindingStageFlags[bindingIndex],
		setLayoutBinding.pImmutableSamplers = nullptr;

		if(spvBinding->type_description->op == SpvOpTypeRuntimeArray)
		{
			setLayoutBinding.descriptorCount = (uint32_t)(-1); //Mark the variable descriptor count
		}

		std::string_view bindingName = spvBinding->name;

		setLayoutBindings.push_back(setLayoutBinding);
		setLayoutBindingNames.push_back(bindingName);
	}


	Span<uint32_t> groupSetSpan =
	{
		.Begin = (uint32_t)mSetLayoutIndices.size(),
		.End   = (uint32_t)(mSetLayoutIndices.size() + bindingSpansPerSet.size())
	};

	mSetLayoutIndexSpansPerShaderGroup[groupName] = groupSetSpan;
	mSetLayoutIndices.resize(mSetLayoutIndices.size() + bindingSpansPerSet.size());


	std::vector<BindingRecord> setLayoutBindingRecords(setLayoutBindingNames.size());
	for(uint32_t bindingSpanIndex = 0; bindingSpanIndex < (uint32_t)bindingSpansPerSet.size(); bindingSpanIndex++)
	{
		Span<uint32_t> bindingSpan = bindingSpansPerSet[bindingSpanIndex];

		std::span<std::string_view> bindingNameSpan   = std::span(setLayoutBindingNames.begin()   + bindingSpan.Begin, setLayoutBindingNames.begin()   + bindingSpan.End);
		std::span<BindingRecord>    bindingRecordSpan = std::span(setLayoutBindingRecords.begin() + bindingSpan.Begin, setLayoutBindingRecords.begin() + bindingSpan.End);

		uint16_t setDomain = DetectSetDomain(bindingNameSpan, bindingRecordSpan);
		assert(setDomain != UndefinedSetDomain);

		std::span<VkDescriptorSetLayoutBinding> bindingInfoSpan = std::span(setLayoutBindings.begin() + bindingSpan.Begin, setLayoutBindings.begin() + bindingSpan.End);
		mSetLayoutIndices[groupSetSpan.Begin + bindingSpanIndex] = RegisterSetLayout(setDomain, bindingInfoSpan, bindingRecordSpan);
	}
}

void Vulkan::ShaderDatabase::RegisterPushConstants(std::string_view groupName, const std::span<std::wstring> shaderModuleNames)
{
	std::vector<PushConstantRecord> pushConstantRecords;
	for(const std::wstring& shaderModuleName: shaderModuleNames)
	{
		const spv_reflect::ShaderModule& shaderModule = mLoadedShaderModules.at(shaderModuleName);

		uint32_t pushConstantBlockCount = 0;
		shaderModule.EnumeratePushConstantBlocks(&pushConstantBlockCount, nullptr);

		std::vector<SpvReflectBlockVariable*> pushConstantBlocks(pushConstantBlockCount);
		shaderModule.EnumeratePushConstantBlocks(&pushConstantBlockCount, pushConstantBlocks.data());

		VkShaderStageFlags moduleStageFlags = SpvToVkShaderStage(shaderModule.GetShaderStage());

		pushConstantRecords.reserve(pushConstantRecords.size() + pushConstantBlockCount);
		for(SpvReflectBlockVariable* pushConstantBlock: pushConstantBlocks)
		{
			for(uint32_t memberIndex = 0; memberIndex < pushConstantBlock->member_count; pushConstantBlock++)
			{
				const SpvReflectBlockVariable& memberBlock = pushConstantBlock->members[memberIndex];
				assert(memberBlock.size == sizeof(uint32_t)); //We only support 32-bit constants atm

				pushConstantRecords.push_back(PushConstantRecord
				{
					.Name         = memberBlock.name,
					.Offset       = memberBlock.offset,
					.ShaderStages = moduleStageFlags
				});
			}
		}
	}

	std::sort(pushConstantRecords.begin(), pushConstantRecords.end(), [](const PushConstantRecord& left, const PushConstantRecord& right)
	{
		return std::lexicographical_compare(left.Name.begin(), left.Name.end(), right.Name.begin(), right.Name.end());
	});

	uint32_t oldPushConstantCount = (uint32_t)mPushConstantRecords.size();
	
	std::string_view currName = "";
	for(const PushConstantRecord& pushConstantRecord: pushConstantRecords)
	{
		if(currName == pushConstantRecord.Name)
		{
			//Merge and validate push constant record
			assert(mPushConstantRecords.back().Offset == pushConstantRecord.Offset);
			mPushConstantRecords.back().ShaderStages |= pushConstantRecord.ShaderStages;
		}
		else
		{
			mPushConstantRecords.push_back(pushConstantRecord);
		}
	}

	std::sort(pushConstantRecords.begin(), pushConstantRecords.end(), [](const PushConstantRecord& left, const PushConstantRecord& right)
	{
		return left.ShaderStages < right.ShaderStages;
	});

	//Create a list of VkShaderStageFlags for each push constant range. 
	//Since no two ranges can overlap in shader stages, there's no more stages then the maximum different shader types
	uint32_t pushConstantRangeCount = 0;
	std::array<VkShaderStageFlags, std::bit_width((uint32_t)VK_SHADER_STAGE_CALLABLE_BIT_KHR)> pushConstantShaderStages;
	std::fill(pushConstantShaderStages.begin(), pushConstantShaderStages.end(), 0);

	for(const PushConstantRecord& pushConstantRecord: pushConstantRecords)
	{
		//Going through pushConstantShaderStages is as efficient as going through bits of VkShaderStage
		uint32_t rangeIndex = 0;
		while(rangeIndex < pushConstantRangeCount)
		{
			//Propagate the range
			if(pushConstantShaderStages[rangeIndex] & pushConstantRecord.ShaderStages)
			{
				pushConstantShaderStages[rangeIndex] |= pushConstantRecord.ShaderStages;
			}

			rangeIndex++;
		}
	}

	//There can be no more than bits(VkShaderStageFlagBits) of push constant ranges, 
	//since VkPipelineLayoutCreateInfo requires ranges to have non-overlapping shader stages
	uint32_t pushConstantRangeCount = 0;
	std::array<VkPushConstantRange, sizeof(VkShaderStageFlagBits) * CHAR_BIT> pushConstantRanges;

	//Range - A number of constants with overlapping shaders
	for(const PushConstantRecord& pushConstantRecord: pushConstantRecords)
	{
		VkPushConstantRange* rangeToChange = nullptr;
		for(uint32_t rangeIndex = 0; rangeIndex < pushConstantRangeCount; rangeIndex++)
		{
			if(pushConstantRanges[rangeIndex].stageFlags == pushConstantRecord.ShaderStages)
			{
				rangeToChange = &pushConstantRanges[rangeIndex];
			}
		}

		if(rangeToChange == nullptr) //No exact match found, try to merge
		{
			uint32_t firstOverlapIndex = 0;
			while(firstOverlapIndex < pushConstantRangeCount)
			{
				if(pushConstantRanges[firstOverlapIndex].stageFlags & pushConstantRecord.ShaderStages)
				{
					break;
				}

				firstOverlapIndex++;
			}

			if(firstOverlapIndex != pushConstantRangeCount)
			{
				rangeToChange = &pushConstantRanges[firstOverlapIndex];
				rangeToChange->stageFlags |= pushConstantRecord.ShaderStages;

				//Merge all overlapping ranges starting at firstOverlapIndex, deleting overlapping ones and shifting remaining ones
				//Only check for stage flags to overlap
				uint32_t shiftCount = 0;
				for(uint32_t rangeIndex = firstOverlapIndex + 1; rangeIndex < pushConstantRangeCount; rangeIndex++)
				{
					if(rangeToChange->stageFlags & pushConstantRanges[rangeIndex].stageFlags)
					{
						uint32_t fixedRangeBegin = rangeToChange->offset;
						uint32_t fixedRangeEnd   = rangeToChange->offset + rangeToChange->size;
						
						uint32_t mergedRangeBegin = pushConstantRanges[rangeIndex].offset;
						uint32_t mergedRangeEnd   = pushConstantRanges[rangeIndex].offset + pushConstantRanges[rangeIndex].size;

						uint32_t newBegin = std::min(fixedRangeBegin, mergedRangeBegin);
						uint32_t newEnd   = std::max(fixedRangeEnd,   mergedRangeEnd);

						rangeToChange->stageFlags |= pushConstantRanges[rangeIndex].stageFlags;
						rangeToChange->offset      = newBegin;
						rangeToChange->size        = newEnd - newBegin;

						shiftCount++;
					}

					pushConstantRanges[rangeIndex - shiftCount] = pushConstantRanges[rangeIndex];
				}

				pushConstantRangeCount -= shiftCount;
			}
			else
			{
				//Add a new range
				pushConstantRanges[pushConstantRangeCount] = VkPushConstantRange
				{
					.stageFlags = pushConstantRecord.ShaderStages,
					.offset     = pushConstantRecord.Offset,
					.size       = sizeof(uint32_t)
				};

				rangeToChange = &pushConstantRanges[pushConstantRangeCount];

				pushConstantRangeCount++;
			}
		}

		assert(rangeToChange != nullptr);

		uint32_t rangeBegin = rangeToChange->offset;
		uint32_t rangeEnd   = rangeToChange->offset + rangeToChange->size;
						
		uint32_t recordBegin = pushConstantRecord.Offset;
		uint32_t recordEnd   = pushConstantRecord.Offset + sizeof(uint32_t);

		uint32_t newRangeBegin = std::min(rangeBegin, recordBegin);
		uint32_t newRangeEnd   = std::max(rangeEnd,   recordEnd);

		rangeToChange->offset = newRangeBegin;
		rangeToChange->size   = newRangeEnd;
	}

	uint32_t oldRangeCount = (uint32_t)mPushConstantRanges.size();
	mPushConstantRanges.insert(mPushConstantRanges.end(), pushConstantRanges.begin(), pushConstantRanges.begin() + pushConstantRangeCount);

	mPushConstantSpansPerShaderGroup[groupName] = PushConstantSpans
	{
		.RecordSpan =
		{
			.Begin = oldPushConstantCount,
			.End   = (uint32_t)mPushConstantRecords.size()
		},

		.RangeSpan =
		{
			.Begin = oldRangeCount,
			.End   = (uint32_t)mPushConstantRanges.size()
		}
	};
}

void Vulkan::ShaderDatabase::SplitSetsByPivot(uint32_t existingSetCount, std::vector<SpvReflectDescriptorSet*>& inoutSpvSets, std::span<SpvReflectDescriptorSet*>* outExistingSetSpan, std::span<SpvReflectDescriptorSet*>* outNewSetSpan)
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

void Vulkan::ShaderDatabase::CalculateUpdatedSetSizes(const std::span<SpvReflectDescriptorSet*> moduleUpdatedSets, const std::vector<Span<uint32_t>>& existingBindingSpansPerSet, std::vector<uint32_t>& outUpdatedSetSizes)
{
	outUpdatedSetSizes.reserve(moduleUpdatedSets.size());
	for(SpvReflectDescriptorSet* moduleSet: moduleUpdatedSets)
	{
		uint32_t setSize = existingBindingSpansPerSet[moduleSet->set].End - existingBindingSpansPerSet[moduleSet->set].Begin;
		for(uint32_t bindingIndex = 0; bindingIndex < moduleSet->binding_count; bindingIndex++)
		{
			setSize = std::max(setSize, moduleSet->bindings[bindingIndex]->binding + 1);
		}

		outUpdatedSetSizes.push_back(setSize);
	}
}

void Vulkan::ShaderDatabase::MergeExistingSetBindings(const std::span<SpvReflectDescriptorSet*> setUpdates, std::vector<SpvReflectDescriptorBinding*>& inoutBindings, std::vector<Span<uint32_t>>& inoutSetSpans)
{
	std::vector<uint32_t> moduleSetSizes;
	CalculateUpdatedSetSizes(setUpdates, inoutSetSpans, moduleSetSizes);

	uint32_t firstSetIndexToUpdate = (uint32_t)inoutSetSpans.size();
	for(size_t moduleSetIndex = 0; moduleSetIndex < setUpdates.size(); moduleSetIndex++)
	{
		SpvReflectDescriptorSet* moduleSet     = setUpdates[moduleSetIndex];
		uint32_t                 moduleSetSize = moduleSetSizes[moduleSetIndex];

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
		SpvReflectDescriptorSet* moduleSet     = setUpdates[0];
		uint32_t                 moduleSetSize = moduleSetSizes[0];

		uint32_t recordedSetSize = inoutSetSpans.back().End - inoutSetSpans.back().Begin;
		uint32_t sizeDiff        = moduleSetSize - recordedSetSize;

		//Just append the data for the last set to the end, only the last set is changed
		inoutBindings.resize(inoutBindings.size() + sizeDiff, nullptr);
		inoutSetSpans.back().End = inoutSetSpans.back().End + sizeDiff;
	}
	else if(firstSetIndexToUpdate < inoutSetSpans.size())
	{
		//Recreate the set binding data
		std::vector<SpvReflectDescriptorBinding*> newSetLayoutBindings;
		newSetLayoutBindings.reserve(inoutBindings.size());

		//Copy the bindings that don't need to be re-sorted
		uint32_t preservedBindingCount = inoutSetSpans[firstSetIndexToUpdate].Begin;
		newSetLayoutBindings.resize(preservedBindingCount);
		memcpy(newSetLayoutBindings.data(), inoutBindings.data(), preservedBindingCount);

		//Recalculate new set sizes. To do it in-place, first shift the beginning points of spans left while leaving end points untouched
		for(size_t moduleSetIndex = 0; moduleSetIndex < setUpdates.size(); moduleSetIndex++)
		{
			SpvReflectDescriptorSet* moduleSet = setUpdates[moduleSetIndex];
			if(moduleSet->set >= firstSetIndexToUpdate)
			{
				uint32_t moduleSetSize   = moduleSetSizes[moduleSetIndex];
				uint32_t recordedSetSize = inoutSetSpans[moduleSet->set].End - inoutSetSpans[moduleSet->set].Begin;

				uint32_t sizeDiff = 0;	
				if(recordedSetSize < moduleSetSize)
				{
					sizeDiff = moduleSetSize - recordedSetSize;
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

			newSetLayoutBindings.resize(newSetLayoutBindings.size() + newSpanSize, nullptr);
			memcpy(newSetLayoutBindings.data() + newSpan.Begin, inoutBindings.data() + originalSpan.Begin, originalSpanSize);
		}

		inoutBindings.swap(newSetLayoutBindings);
	}

	for(auto moduleSet: setUpdates)
	{
		size_t bindingStart = inoutSetSpans[moduleSet->set].Begin;
		for(uint32_t moduleBindingIndex = 0; moduleBindingIndex < moduleSet->binding_count; moduleBindingIndex++)
		{
			SpvReflectDescriptorBinding* moduleBinding   = moduleSet->bindings[moduleBindingIndex];
			SpvReflectDescriptorBinding* bindingToUpdate = inoutBindings[bindingStart + moduleBinding->binding];

			if(bindingToUpdate == nullptr)
			{
				inoutBindings[bindingStart + moduleBinding->binding] = moduleBinding;
			}
			else
			{
				assert(strcmp(bindingToUpdate->name, moduleBinding->name) == 0);

				assert(bindingToUpdate->binding         == moduleBinding->binding);
				assert(bindingToUpdate->descriptor_type == moduleBinding->descriptor_type);
				assert(bindingToUpdate->count           == moduleBinding->count);
			}
		}
	}
}

void Vulkan::ShaderDatabase::MergeNewSetBindings(const std::span<SpvReflectDescriptorSet*> newSets, std::vector<SpvReflectDescriptorBinding*>& inoutBindings, std::vector<Span<uint32_t>>& inoutSetSpans)
{
	uint32_t oldBindingCount = (uint32_t)inoutBindings.size();
	uint32_t newBindingCount = std::accumulate(newSets.begin(), newSets.end(), oldBindingCount);
	inoutBindings.resize(newBindingCount, nullptr);

	uint32_t oldSetCount = (uint32_t)inoutSetSpans.size();
	uint32_t newSetCount = oldSetCount;
	for(auto newSet: newSets)
	{
		newSetCount = std::max(newSetCount, newSet->set + 1);
	}

	inoutSetSpans.resize(newSetCount, Span<uint32_t>{.Begin = 0, .End = 0});
	for(auto newSet: newSets)
	{
		uint32_t newSetSize = 0;
		for(uint32_t bindingIndex = 0; bindingIndex < newSet->binding_count; bindingIndex++)
		{
			newSetSize = std::max(newSetSize, newSet->bindings[bindingIndex]->binding + 1);
		}

		Span<uint32_t> bindingSpan = Span<uint32_t>
		{
			.Begin = oldBindingCount,
			.End   = oldBindingCount + newSetSize
		};

		inoutSetSpans[newSet->set] = bindingSpan;
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
		size_t bindingStart = inoutSetSpans[newSet->set].Begin;
		for(uint32_t moduleBindingIndex = 0; moduleBindingIndex < newSet->binding_count; moduleBindingIndex++)
		{
			SpvReflectDescriptorBinding* moduleBinding = newSet->bindings[moduleBindingIndex];
			inoutBindings[bindingStart + moduleBinding->binding] = moduleBinding;
		}
	}
}

uint32_t Vulkan::ShaderDatabase::RegisterSetLayout(uint16_t setDomain, std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<BindingRecord> setBindingRecords)
{
	//Traverse the linked list of binding spans for the domain
	uint32_t* layoutNodeIndex = &mLayoutHeadNodeIndicesPerDomain[setDomain];
	while(*layoutNodeIndex != (uint32_t)(-1))
	{
		SetLayoutRecordNode layoutNode = mSetLayoutNodes[*layoutNodeIndex];

		uint32_t layoutBindingCount = layoutNode.BindingSpan.Begin - layoutNode.BindingSpan.End;
		if(layoutBindingCount == (uint32_t)setBindings.size())
		{
			//Try to match the set
			uint32_t layoutBindingIndex = 0;
			while(layoutBindingIndex < layoutBindingCount)
			{
				if(mLayoutBindingTypesFlat[(size_t)layoutNode.BindingSpan.Begin + layoutBindingIndex] != setBindingRecords[layoutBindingIndex].Type)
				{
					//Binding types don't match
					break;
				}
			}

			if(layoutBindingIndex == layoutBindingCount)
			{
				//All binding types matched
				break;
			}
		}

		layoutNodeIndex = &layoutNode.NextNodeIndex;
	}
	
	if(*layoutNodeIndex == (uint32_t)-1)
	{
		//Allocate the space for new bindings
		size_t registeredBindingCount = mLayoutBindingsFlat.size();
		size_t addedBindingCount      = setBindings.size();

		SetLayoutRecordNode newSetBindingNode;
		newSetBindingNode.BindingSpan.Begin = (uint32_t)registeredBindingCount;
		newSetBindingNode.BindingSpan.End   = (uint32_t)(registeredBindingCount + addedBindingCount);
		newSetBindingNode.SetLayout         = nullptr;
		newSetBindingNode.NextNodeIndex     = (uint16_t)-1;

		mSetLayoutNodes.push_back(newSetBindingNode);
		uint32_t newNodeIndex = (mSetLayoutNodes.size() - 1);
		*layoutNodeIndex = newNodeIndex;
		
		mLayoutBindingsFlat.resize(registeredBindingCount + addedBindingCount);
		mLayoutBindingFlagsFlat.resize(registeredBindingCount + addedBindingCount);
		mLayoutBindingTypesFlat.resize(registeredBindingCount + addedBindingCount);
		mLayoutBindingFrameCountsFlat.resize(registeredBindingCount + addedBindingCount);

		//Validate bindings
		for(uint32_t bindingIndex = newSetBindingNode.BindingSpan.Begin; bindingIndex < newSetBindingNode.BindingSpan.End; bindingIndex++)
		{
			uint32_t layoutBindingIndex = bindingIndex - newSetBindingNode.BindingSpan.Begin;
	
			const VkDescriptorSetLayoutBinding& bindingInfo   = setBindings[layoutBindingIndex];
			const BindingRecord&                bindingRecord = setBindingRecords[layoutBindingIndex];

			assert(ValidateNewBinding(bindingInfo, bindingRecord.DescriptorType, bindingRecord.DescriptorFlags));
			assert(bindingInfo.descriptorCount != (uint32_t)(-1) || bindingInfo.binding == (setBindings.size() - 1)); //Only the last binding can be variable sized
			assert(bindingInfo.binding == layoutBindingIndex);

			mLayoutBindingsFlat[layoutBindingIndex] = bindingInfo;
			if(setDomain == SharedSetDomain && bindingRecord.Type == (uint16_t)SharedDescriptorDatabase::SharedDescriptorBindingType::SamplerList)
			{
				mLayoutBindingsFlat[layoutBindingIndex].pImmutableSamplers = mSamplerManagerRef->GetSamplerVariableArray();
			}
		}

		for(uint32_t bindingIndex = newSetBindingNode.BindingSpan.Begin; bindingIndex < newSetBindingNode.BindingSpan.End; bindingIndex++)
		{
			uint32_t layoutBindingIndex = bindingIndex - newSetBindingNode.BindingSpan.Begin;

			const BindingRecord& bindingRecord = setBindingRecords[layoutBindingIndex];
			mLayoutBindingTypesFlat[layoutBindingIndex] = bindingRecord.Type;
		}

		for(uint32_t bindingIndex = newSetBindingNode.BindingSpan.Begin; bindingIndex < newSetBindingNode.BindingSpan.End; bindingIndex++)
		{
			uint32_t layoutBindingIndex = bindingIndex - newSetBindingNode.BindingSpan.Begin;

			const BindingRecord& bindingRecord = setBindingRecords[layoutBindingIndex];
			mLayoutBindingFlagsFlat[layoutBindingIndex] = bindingRecord.DescriptorFlags;
		}

		for(uint32_t bindingIndex = newSetBindingNode.BindingSpan.Begin; bindingIndex < newSetBindingNode.BindingSpan.End; bindingIndex++)
		{
			uint32_t layoutBindingIndex = bindingIndex - newSetBindingNode.BindingSpan.Begin;

			const BindingRecord& bindingRecord = setBindingRecords[layoutBindingIndex];
			mLayoutBindingFrameCountsFlat[layoutBindingIndex] = bindingRecord.FrameCount;
		}
	}
	else
	{
		Span<uint32_t> layoutBindingSpan = mSetLayoutNodes[*layoutNodeIndex].BindingSpan;

		VkShaderStageFlags setStageFlags;
		for(uint32_t bindingIndex = 0; bindingIndex < setBindings.size(); bindingIndex++)
		{
			const VkDescriptorSetLayoutBinding& newBindingInfo = setBindings[bindingIndex];
			VkDescriptorSetLayoutBinding& existingBindingInfo  = mLayoutBindingsFlat[(size_t)layoutBindingSpan.Begin + bindingIndex];

			assert(ValidateExistingBinding(newBindingInfo, existingBindingInfo));
			assert(newBindingInfo.binding == bindingIndex);

			existingBindingInfo.stageFlags |= newBindingInfo.stageFlags;
		}
	}

	return *layoutNodeIndex;
}

uint16_t Vulkan::ShaderDatabase::DetectSetDomain(std::span<std::string_view> bindingNames, std::span<BindingRecord> outSetBindingRecords)
{
	assert(bindingNames.size() == outSetBindingRecords.size());
	if(bindingNames.size() == 0)
	{
		return UndefinedSetDomain;
	}

	outSetBindingRecords[0] = mBindingRecordMap[bindingNames[0]];
	for(size_t bindingIndex = 1; bindingIndex < bindingNames.size(); bindingIndex++)
	{
		BindingRecord bindingRecord = mBindingRecordMap[bindingNames[bindingIndex]];
		if(bindingRecord.Domain != outSetBindingRecords[0].Domain)
		{
			//All bindings in the set should belong to the same domain
			return UndefinedSetDomain;
		}
		else
		{
			outSetBindingRecords[bindingIndex] = bindingRecord;
		}
	}

	return outSetBindingRecords[0].Domain;
}

void Vulkan::ShaderDatabase::BuildSetLayouts()
{
	size_t layoutCount       = mSetLayoutNodes.size();
	size_t totalBindingCount = mLayoutBindingsFlat.size();

	for(size_t layoutIndex = 0; layoutIndex < layoutCount; layoutIndex++)
	{
		SetLayoutRecordNode& layoutRecordNode   = mSetLayoutNodes[layoutIndex];
		uint32_t             layoutBindingCount = layoutRecordNode.BindingSpan.End - layoutRecordNode.BindingSpan.Begin;

		VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo =
		{
			.pNext         = nullptr,
			.bindingCount  = layoutBindingCount,
			.pBindingFlags = mLayoutBindingFlagsFlat.data() + layoutRecordNode.BindingSpan.Begin
		};

		vgs::GenericStructureChain<VkDescriptorSetLayoutCreateInfo> setLayoutCreateInfoChain;
		setLayoutCreateInfoChain.AppendToChain(bindingFlagsCreateInfo);

		VkDescriptorSetLayoutCreateInfo& setLayoutCreateInfo = setLayoutCreateInfoChain.GetChainHead();
		setLayoutCreateInfo.flags        = 0;
		setLayoutCreateInfo.pBindings    = mLayoutBindingsFlat.data() + layoutRecordNode.BindingSpan.Begin;
		setLayoutCreateInfo.bindingCount = layoutBindingCount;

		ThrowIfFailed(vkCreateDescriptorSetLayout(mDeviceRef, &setLayoutCreateInfo, nullptr, &layoutRecordNode.SetLayout));
	}
}

bool Vulkan::ShaderDatabase::ValidateNewBinding(const VkDescriptorSetLayoutBinding& bindingInfo, VkDescriptorType expectedDescriptorType, VkDescriptorBindingFlags expectedDescriptorFlags) const
{
	if(bindingInfo.descriptorType != expectedDescriptorType)
	{
		return false;
	}

	if(bindingInfo.descriptorCount == (uint32_t)(-1) && (expectedDescriptorFlags & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) == 0)
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