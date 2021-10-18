#include "VulkanShaders.hpp"
#include "VulkanUtils.hpp"
#include "VulkanFunctions.hpp"
#include "VulkanSamplers.hpp"
#include "VulkanDescriptorsMisc.hpp"
#include "VulkanDeviceParameters.hpp"
#include "VulkanSharedDescriptorDatabaseBuilder.hpp"
#include "VulkanPassDescriptorDatabaseBuilder.hpp"
#include "FrameGraph/VulkanRenderPassDispatchFuncs.hpp"
#include "../../Core/Util.hpp"
#include "../../Logging/LoggerQueue.hpp"
#include <VulkanGenericStructures.h>
#include <cassert>
#include <unordered_map>
#include <array>
#include <algorithm>

Vulkan::ShaderDatabase::ShaderDatabase(VkDevice device, SamplerManager* samplerManager, LoggerQueue* logger): mLogger(logger), mDeviceRef(device), mSamplerManagerRef(samplerManager)
{
	//Initialize shared binding records
	mDomainRecords.resize(SharedSetDomain + 1);
	mDomainRecords[SharedSetDomain] = DomainRecord
	{
		.PassType             = RenderPassType::None,
		.FirstLayoutNodeIndex = (uint32_t)(-1)
	};

	for(uint16_t bindingTypeIndex = 0; bindingTypeIndex < TotalSharedBindings; bindingTypeIndex++)
	{
		std::string_view bindingName = SharedDescriptorBindingNames[bindingTypeIndex];
		mBindingRecordMap[bindingName] = BindingRecord
		{
			.Domain          = SharedSetDomain,
			.Type            = (BindingType)(bindingTypeIndex),
			.DescriptorType  = DescriptorTypesPerSharedBinding[bindingTypeIndex],
			.DescriptorFlags = DescriptorFlagsPerSharedBinding[bindingTypeIndex],
		};
	}
}

Vulkan::ShaderDatabase::~ShaderDatabase()
{
	//Destroy the non-flushed domains
	for(SetLayoutRecordNode& layoutRecordNode: mSetLayoutRecordNodes)
	{
		SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, layoutRecordNode.SetLayout);
	}
}

void Vulkan::ShaderDatabase::RegisterPass(RenderPassType passType)
{
	if(mPassDomainMap.contains(passType))
	{
		return;
	}

	mDomainRecords.push_back(DomainRecord
	{
		.PassType             = passType,
		.FirstLayoutNodeIndex = (uint32_t)(-1)
	});

	BindingDomain newDomain = (BindingDomain)(mDomainRecords.size() - 1);
	mPassDomainMap[passType] = newDomain;

	uint_fast16_t passSubresourceCount = GetPassSubresourceCount(passType);
	for(uint_fast16_t passSubresourceIndex = 0; passSubresourceIndex < passSubresourceCount; passSubresourceIndex++)
	{
		VkDescriptorType bindingDescriptorType = GetPassSubresourceDescriptorType(passType, passSubresourceIndex);
		if(bindingDescriptorType != VK_DESCRIPTOR_TYPE_MAX_ENUM)
		{
			std::string_view bindingName = GetPassSubresourceStringId(passType, passSubresourceIndex);
			mBindingRecordMap[bindingName] = BindingRecord
			{
				.Domain          = newDomain,
				.Type            = (BindingType)passSubresourceIndex,
				.DescriptorType  = bindingDescriptorType,
				.DescriptorFlags = 0
			};
		}
	}
}

void Vulkan::ShaderDatabase::RegisterShaderGroup(std::string_view groupName, std::span<std::wstring> shaderPaths)
{
	assert(!mLayoutNodeRecordIndexSpansPerShaderGroup.contains(groupName));

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

void Vulkan::ShaderDatabase::GetPushConstantInfo(std::string_view groupName, std::string_view pushConstantName, uint32_t* outPushConstantOffset, VkShaderStageFlags* outShaderStages) const
{
	Span<uint32_t> pushConstantSpan = mPushConstantSpansPerShaderGroup.at(groupName).RecordSpan;

	const auto rangeBegin = mPushConstantRecordsFlat.begin() + pushConstantSpan.Begin;
	const auto rangeEnd   = mPushConstantRecordsFlat.begin() + pushConstantSpan.End;
	auto pushConstantRecord = std::lower_bound(rangeBegin, rangeEnd, pushConstantName, [](const PushConstantRecord& left, const std::string_view& right)
	{
		return left.Name < right;
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

void Vulkan::ShaderDatabase::CreateMatchingPipelineLayout(std::span<std::string_view> groupNamesForSets, std::span<std::string_view> groupNamesForPushConstants, VkPipelineLayout* outPipelineLayout) const
{
	assert(outPipelineLayout != nullptr);

	Span<uint32_t> matchingSetLayoutSpan         = FindMatchingSetLayoutSpan(groupNamesForSets);
	Span<uint32_t> matchingPushConstantRangeSpan = FindMatchingPushConstantRangeSpan(groupNamesForPushConstants);

	if(matchingSetLayoutSpan.End != matchingSetLayoutSpan.Begin && matchingPushConstantRangeSpan.End != matchingPushConstantRangeSpan.Begin)
	{
		std::span setLayouts    = {mSetLayoutsForCreatePipelineFlat.begin() + matchingSetLayoutSpan.Begin,         mSetLayoutsForCreatePipelineFlat.begin() + matchingSetLayoutSpan.End};
		std::span pushConstants = {mPushConstantRangesFlat.begin()          + matchingPushConstantRangeSpan.Begin, mPushConstantRangesFlat.begin()          + matchingPushConstantRangeSpan.End};

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
		pipelineLayoutCreateInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.pNext                  = nullptr;
		pipelineLayoutCreateInfo.flags                  = 0;
		pipelineLayoutCreateInfo.setLayoutCount         = (uint32_t)setLayouts.size();
		pipelineLayoutCreateInfo.pSetLayouts            = setLayouts.data();
		pipelineLayoutCreateInfo.pushConstantRangeCount = (uint32_t)pushConstants.size();
		pipelineLayoutCreateInfo.pPushConstantRanges    = pushConstants.data();

		ThrowIfFailed(vkCreatePipelineLayout(mDeviceRef, &pipelineLayoutCreateInfo, nullptr, outPipelineLayout));
	}
	else
	{
		*outPipelineLayout = VK_NULL_HANDLE;
	}
}

std::span<VkDescriptorSet> Vulkan::ShaderDatabase::AssignPassSets(uint32_t passIndex, uint32_t frame, std::span<const std::string_view> shaderGroupSequence, std::span<DescriptorSetBindRange> outBindRangesPerGroup)
{
	assert(shaderGroupSequence.size() == outBindRangesPerGroup.size());
	uint32_t oldLayoutIndexCount = (uint32_t)mSetLayoutIndicesForGroupSequences.size();

	//Try to match each shader group layout span with currentLayoutRecordIndices, and find the first non-matching element
	std::vector<uint32_t> currentLayoutRecordIndices;
	for(uint32_t shaderGroupIndex = 0; shaderGroupIndex < shaderGroupSequence.size(); shaderGroupIndex++)
	{
		const std::string_view shaderGroupName = shaderGroupSequence[shaderGroupIndex];
		Span<uint32_t> groupLayoutSpan = mLayoutNodeRecordIndexSpansPerShaderGroup.at(shaderGroupName);

		uint32_t currIndexToMatch = 0;
		const std::span<const uint32_t> groupLayoutRecordIndices = {mLayoutRecordNodeIndicesFlat.begin() + groupLayoutSpan.Begin, mLayoutRecordNodeIndicesFlat.begin() + groupLayoutSpan.End};
		while(currIndexToMatch < groupLayoutRecordIndices.size() && currIndexToMatch < currentLayoutRecordIndices.size())
		{
			if(groupLayoutRecordIndices[currIndexToMatch] != currentLayoutRecordIndices[currIndexToMatch])
			{
				break;
			}

			currIndexToMatch++;
		}

		uint32_t prevLayoutIndexCount = (uint32_t)mSetLayoutIndicesForGroupSequences.size() - oldLayoutIndexCount;

		outBindRangesPerGroup[shaderGroupIndex].Begin     = prevLayoutIndexCount;
		outBindRangesPerGroup[shaderGroupIndex].End       = prevLayoutIndexCount + (uint32_t)(groupLayoutRecordIndices.size() - currIndexToMatch);
		outBindRangesPerGroup[shaderGroupIndex].BindPoint = currIndexToMatch;

		mSetLayoutIndicesForGroupSequences.insert(mSetLayoutIndicesForGroupSequences.end(), groupLayoutRecordIndices.begin() + currIndexToMatch, groupLayoutRecordIndices.end());

		currentLayoutRecordIndices.resize(groupLayoutRecordIndices.size());
		std::copy(groupLayoutRecordIndices.begin() + currIndexToMatch, groupLayoutRecordIndices.end(), currentLayoutRecordIndices.begin() + currIndexToMatch);
	}

	uint32_t newLayoutIndexCount = (uint32_t)mSetLayoutIndicesForGroupSequences.size();
	mSetSpanMetadatasPerGroupSequences.push_back(DescriptorSetSpanMetadata
	{
		.PassId             = passIndex,
		.FrameResourceIndex = frame,
		.SetLayoutNodeSpan  = Span<uint32_t>
		{
			.Begin = oldLayoutIndexCount,
			.End   = newLayoutIndexCount
		}
	});

	//Since std::span is gonna be invalidated within many calls to AssignPassSets, return mock span starting from NULL 
	//(We only need to know the offset and size at this point, not the final data pointer)
	VkDescriptorSet* newDescriptorRecordStart = GetOriginalDescriptorSpanStart() + oldLayoutIndexCount;
	size_t           newDescriptorRecordCount = (size_t)newLayoutIndexCount - oldLayoutIndexCount;
	return std::span((VkDescriptorSet*)newDescriptorRecordStart, (VkDescriptorSet*)newDescriptorRecordStart + newDescriptorRecordCount);
}

void Vulkan::ShaderDatabase::FlushDescriptorSetData(SharedDescriptorDatabaseBuilder* sharedDatabaseBuilder, PassDescriptorDatabaseBuilder* passDatabaseBuilder)
{
	sharedDatabaseBuilder->ClearRegisteredSharedSetInfos();

	DomainRecord sharedDomainRecord = mDomainRecords[SharedSetDomain];
	std::unordered_map<uint32_t, uint32_t> sharedLayoutFlushedRecords;

	//Flush shared set layouts
	uint32_t sharedLayoutNodeIndex = sharedDomainRecord.FirstLayoutNodeIndex;
	while(sharedLayoutNodeIndex != (uint32_t)(-1))
	{
		SetLayoutRecordNode& setLayoutNode = mSetLayoutRecordNodes[sharedLayoutNodeIndex];
		std::span<uint16_t> setLayoutBindings = {mLayoutBindingTypesFlat.begin() + setLayoutNode.BindingSpan.Begin, mLayoutBindingTypesFlat.begin() + setLayoutNode.BindingSpan.End};

		sharedLayoutFlushedRecords[sharedLayoutNodeIndex] = sharedDatabaseBuilder->RegisterSharedSetInfo(setLayoutNode.SetLayout, setLayoutBindings);
		sharedLayoutNodeIndex = setLayoutNode.NextNodeIndex;

		setLayoutNode.SetLayout = VK_NULL_HANDLE; //Ownership is transferred
	}

	//Clear the linked list. No memory is freed, but the ownership of set layouts is taken away
	mDomainRecords[SharedSetDomain].FirstLayoutNodeIndex = (uint32_t)(-1);

	for(const DescriptorSetSpanMetadata& setSpanMetadata: mSetSpanMetadatasPerGroupSequences)
	{
		for(uint32_t nodeIndexIndex = setSpanMetadata.SetLayoutNodeSpan.Begin; nodeIndexIndex < setSpanMetadata.SetLayoutNodeSpan.End; nodeIndexIndex++)
		{
			uint32_t layoutRecordIndex = mSetLayoutIndicesForGroupSequences[nodeIndexIndex];

			auto sharedLayoutRecordIt = sharedLayoutFlushedRecords.find(layoutRecordIndex);
			if(sharedLayoutRecordIt != sharedLayoutFlushedRecords.end())
			{
				sharedDatabaseBuilder->AddSharedSetInfo(sharedLayoutRecordIt->second, setSpanMetadata.FrameResourceIndex);
			}
			else
			{
				const SetLayoutRecordNode& setLayoutRecord = mSetLayoutRecordNodes[layoutRecordIndex];
				std::span setLayoutBindingSpan = { mLayoutBindingTypesFlat.begin() + setLayoutRecord.BindingSpan.Begin, mLayoutBindingTypesFlat.begin() + setLayoutRecord.BindingSpan.End };
				passDatabaseBuilder->AddPassSetInfo(setLayoutRecord.SetLayout, setSpanMetadata.PassId, setLayoutBindingSpan);
			}
		}
	}
}

VkDescriptorSet* Vulkan::ShaderDatabase::GetOriginalDescriptorSpanStart() const
{
	return nullptr;
}

void Vulkan::ShaderDatabase::RegisterBindings(const std::string_view groupName, const std::span<std::wstring> shaderModuleNames)
{
	std::vector<Span<uint32_t>> prevBindingSpansPerSet;
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

		//Move previous sets. The loop will wrap around 0
		for(size_t setIndex = prevBindingSpansPerSet.size() - 1; setIndex < prevBindingSpansPerSet.size(); setIndex--)
		{
			Span<uint32_t> prevSpan = prevBindingSpansPerSet[setIndex];
			Span<uint32_t> currSpan = bindingSpansPerSet[setIndex];

			uint32_t prevSpanSize = prevSpan.End - prevSpan.Begin;
			uint32_t currSpanSize = currSpan.End - currSpan.Begin;

			for(uint32_t spanBindingIndex = 0; spanBindingIndex < prevSpanSize; spanBindingIndex++)
			{
				bindingStageFlags[currSpan.Begin + spanBindingIndex] = bindingStageFlags[prevSpan.Begin + spanBindingIndex];
			}

			for(uint32_t spanBindingIndex = prevSpanSize; spanBindingIndex < currSpanSize; spanBindingIndex++)
			{
				bindingStageFlags[currSpan.Begin + spanBindingIndex] = 0;
			}
		}

		for(SpvReflectDescriptorSet* moduleSet: moduleSets)
		{
			Span<uint32_t> setSpan = bindingSpansPerSet[moduleSet->set];
			for(uint32_t bindingIndex = 0; bindingIndex < moduleSet->binding_count; bindingIndex++)
			{
				SpvReflectDescriptorBinding* binding = moduleSet->bindings[bindingIndex];
				bindingStageFlags[setSpan.Begin + binding->binding] |= shaderStage;
			}
		}

		prevBindingSpansPerSet = bindingSpansPerSet;
	}

	//Construct binding list and name list
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
	std::vector<BindingRecord>                bindingRecordsToRegister;
	for(size_t bindingIndex = 0; bindingIndex < spvSetLayoutBindings.size(); bindingIndex++)
	{
		SpvReflectDescriptorBinding* spvBinding = spvSetLayoutBindings[bindingIndex];
		assert(spvBinding != nullptr);

		VkDescriptorSetLayoutBinding setLayoutBinding;
		setLayoutBinding.binding            = spvBinding->binding;
		setLayoutBinding.descriptorType     = SpvToVkDescriptorType(spvBinding->descriptor_type);
		setLayoutBinding.descriptorCount    = spvBinding->count;
		setLayoutBinding.stageFlags         = bindingStageFlags[bindingIndex];
		setLayoutBinding.pImmutableSamplers = nullptr;

		if(spvBinding->type_description->op == SpvOpTypeRuntimeArray)
		{
			setLayoutBinding.descriptorCount = (uint32_t)(-1); //Mark the variable descriptor count
		}

		setLayoutBindings.push_back(setLayoutBinding);
		bindingRecordsToRegister.push_back(mBindingRecordMap.at(spvBinding->name));
	}


	Span<uint32_t> groupSetSpan =
	{
		.Begin = (uint32_t)mLayoutRecordNodeIndicesFlat.size(),
		.End   = (uint32_t)(mLayoutRecordNodeIndicesFlat.size() + bindingSpansPerSet.size())
	};

	mLayoutNodeRecordIndexSpansPerShaderGroup[groupName] = groupSetSpan;
	mLayoutRecordNodeIndicesFlat.resize(mLayoutRecordNodeIndicesFlat.size() + bindingSpansPerSet.size(), (uint32_t)(-1));

	for(uint32_t bindingSpanIndex = 0; bindingSpanIndex < (uint32_t)bindingSpansPerSet.size(); bindingSpanIndex++)
	{
		Span<uint32_t> bindingSpan = bindingSpansPerSet[bindingSpanIndex];

		std::span<VkDescriptorSetLayoutBinding> bindingInfoSpan   = std::span(setLayoutBindings.begin()        + bindingSpan.Begin, setLayoutBindings.begin()        + bindingSpan.End);
		std::span<BindingRecord>                bindingRecordSpan = std::span(bindingRecordsToRegister.begin() + bindingSpan.Begin, bindingRecordsToRegister.begin() + bindingSpan.End);

		uint16_t setDomain = ValidateSetDomain(bindingRecordSpan);
		assert(setDomain != UndefinedSetDomain);

		mLayoutRecordNodeIndicesFlat[groupSetSpan.Begin + bindingSpanIndex] = RegisterSetLayout(setDomain, bindingInfoSpan, bindingRecordSpan);
	}
}

void Vulkan::ShaderDatabase::RegisterPushConstants(std::string_view groupName, const std::span<std::wstring> shaderModuleNames)
{
	std::vector<PushConstantRecord> pushConstantRecords;
	CollectPushConstantRecords(shaderModuleNames, pushConstantRecords);

	//Sort lexicographically
	std::sort(pushConstantRecords.begin(), pushConstantRecords.end(), [](const PushConstantRecord& left, const PushConstantRecord& right) 
	{
		return std::lexicographical_compare(left.Name.begin(), left.Name.end(), right.Name.begin(), right.Name.end());
	});

	Span<uint32_t> registeredRecords = RegisterPushConstantRecords(pushConstantRecords);
	Span<uint32_t> registeredRanges  = RegisterPushConstantRanges(pushConstantRecords);

	mPushConstantSpansPerShaderGroup[groupName] = PushConstantSpans
	{
		.RecordSpan = registeredRecords,
		.RangeSpan  = registeredRanges
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
		uint32_t moduleSetSize = moduleSetSizes[0];

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
		std::copy(inoutBindings.begin(), inoutBindings.begin() + preservedBindingCount, newSetLayoutBindings.begin());

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
			Span<uint32_t> originalSpan = {.Begin = originalSpanBegin, .End = inoutSetSpans[setIndex].End};
			originalSpanBegin = inoutSetSpans[setIndex].End; //Shift to the next span

			//Calculate the real new span (shifted to newSpanBegin)
			uint32_t       newSpanSize = inoutSetSpans[setIndex].End - inoutSetSpans[setIndex].Begin; //Safe even with unsigned wrap
			Span<uint32_t> newSpan     = {.Begin = (uint32_t)newSetLayoutBindings.size(), .End = (uint32_t)newSetLayoutBindings.size() + newSpanSize};
			inoutSetSpans[setIndex] = newSpan;

			//Copy the already known bindings
			newSetLayoutBindings.resize(newSetLayoutBindings.size() + newSpanSize, nullptr);
			std::copy(inoutBindings.begin() + originalSpan.Begin, inoutBindings.begin() + originalSpan.End, newSetLayoutBindings.begin() + newSpan.Begin);
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
	uint32_t newBindingCount = std::accumulate(newSets.begin(), newSets.end(), oldBindingCount, [](uint32_t acc, SpvReflectDescriptorSet* set)
	{
		return acc + set->binding_count;
	});

	inoutBindings.resize(newBindingCount, nullptr);

	uint32_t oldSetCount = (uint32_t)inoutSetSpans.size();
	uint32_t newSetCount = oldSetCount;
	for(auto newSet: newSets)
	{
		newSetCount = std::max(newSetCount, newSet->set + 1);
	}

	//Allocate spans (only span sizes matter at this point)
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

	//Validate all allocated spans, reordering them (but keeping the size)
	uint32_t spanOffset = oldBindingCount;
	for(uint32_t setIndex = oldSetCount; setIndex < newSetCount; setIndex++)
	{
		uint32_t setSize = inoutSetSpans[setIndex].End - inoutSetSpans[setIndex].Begin;

		inoutSetSpans[setIndex].Begin = spanOffset;
		inoutSetSpans[setIndex].End   = spanOffset + setSize;

		spanOffset = inoutSetSpans[setIndex].End;
	}

	//Fill binding spans
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

void Vulkan::ShaderDatabase::CollectPushConstantRecords(const std::span<std::wstring> shaderModuleNames, std::vector<PushConstantRecord>& outPushConstantRecords)
{
	for(const std::wstring& shaderModuleName: shaderModuleNames)
	{
		const spv_reflect::ShaderModule& shaderModule = mLoadedShaderModules.at(shaderModuleName);

		uint32_t pushConstantBlockCount = 0;
		shaderModule.EnumeratePushConstantBlocks(&pushConstantBlockCount, nullptr);

		std::vector<SpvReflectBlockVariable*> pushConstantBlocks(pushConstantBlockCount);
		shaderModule.EnumeratePushConstantBlocks(&pushConstantBlockCount, pushConstantBlocks.data());

		VkShaderStageFlags moduleStageFlags = SpvToVkShaderStage(shaderModule.GetShaderStage());

		outPushConstantRecords.reserve(outPushConstantRecords.size() + pushConstantBlockCount);
		for(const SpvReflectBlockVariable* pushConstantBlock: pushConstantBlocks)
		{
			for(uint32_t memberIndex = 0; memberIndex < pushConstantBlock->member_count; memberIndex++)
			{
				const SpvReflectBlockVariable& memberBlock = pushConstantBlock->members[memberIndex];
				assert(memberBlock.size == sizeof(uint32_t)); //We only support 32-bit constants atm

				outPushConstantRecords.push_back(PushConstantRecord
				{
					.Name         = memberBlock.name,
					.Offset       = memberBlock.offset,
					.ShaderStages = moduleStageFlags
				});
			}
		}
	}
}

Span<uint32_t> Vulkan::ShaderDatabase::RegisterPushConstantRecords(const std::span<PushConstantRecord> lexicographicallySortedRecords)
{
	uint32_t oldPushConstantCount = (uint32_t)mPushConstantRecordsFlat.size();
	
	std::string_view currName = "";
	for(const PushConstantRecord& pushConstantRecord: lexicographicallySortedRecords)
	{
		if(currName == pushConstantRecord.Name)
		{
			//Merge and validate push constant record
			assert(mPushConstantRecordsFlat.back().Offset == pushConstantRecord.Offset);
			mPushConstantRecordsFlat.back().ShaderStages |= pushConstantRecord.ShaderStages;
		}
		else
		{
			mPushConstantRecordsFlat.push_back(pushConstantRecord);
		}
	}

	return Span<uint32_t>
	{
		.Begin = oldPushConstantCount,
		.End   = (uint32_t)mPushConstantRecordsFlat.size()
	};
}

Span<uint32_t> Vulkan::ShaderDatabase::RegisterPushConstantRanges(const std::span<PushConstantRecord> shaderSortedRecords)
{
	//There can be no more than bits(VkShaderStageFlagBits) of push constant ranges, 
	//since VkPipelineLayoutCreateInfo requires ranges to have non-overlapping shader stages
	uint32_t pushConstantRangeCount = 0;
	std::array<VkPushConstantRange, sizeof(VkShaderStageFlagBits) * CHAR_BIT> pushConstantRanges;
	std::fill(pushConstantRanges.begin(), pushConstantRanges.end(), VkPushConstantRange
	{
		.stageFlags = 0,
		.offset     = 0,
		.size       = 0
	});

	//Range - A number of constants with overlapping shaders
	for(const PushConstantRecord& pushConstantRecord: shaderSortedRecords)
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

	uint32_t oldRangeCount = (uint32_t)mPushConstantRangesFlat.size();
	mPushConstantRangesFlat.insert(mPushConstantRangesFlat.end(), pushConstantRanges.begin(), pushConstantRanges.begin() + pushConstantRangeCount);

	return Span<uint32_t>
	{
		.Begin = oldRangeCount,
		.End   = (uint32_t)mPushConstantRangesFlat.size()
	};
}

Span<uint32_t> Vulkan::ShaderDatabase::FindMatchingSetLayoutSpan(std::span<std::string_view> groupNames) const
{
	if(groupNames.size() == 0)
	{
		return Span<uint32_t>
		{
			.Begin = 0,
			.End   = 0
		};
	}

	Span<uint32_t> handledLayoutSpan = mLayoutNodeRecordIndexSpansPerShaderGroup.at(groupNames[0]);
	for(size_t groupIndex = 1; groupIndex < groupNames.size(); groupIndex++)
	{
		std::string_view groupName = groupNames[groupIndex];

		Span<uint32_t> testLayoutSpan = mLayoutNodeRecordIndexSpansPerShaderGroup.at(groupName);
		if(testLayoutSpan.Begin == handledLayoutSpan.Begin)
		{
			//The layout objects themselves match, just create the minimal span satisfying both
			handledLayoutSpan.End = std::max(handledLayoutSpan.End, testLayoutSpan.End);
		}
		else
		{
			//Either allow all layouts of handledLayoutSpan be included in layouts of testLayoutSpan, or the other way around
			//Allowing partial intersection (some are included one way, some the other way) will require creating new set layout spans
			enum class LayoutAffinity {NoAffinity, HandledAffinity, TestAffinity};
			LayoutAffinity affinity = LayoutAffinity::NoAffinity;

			//Try to match the layouts by checking the binding infos
			uint32_t handledLayoutCount = handledLayoutSpan.End - handledLayoutSpan.Begin;
			uint32_t testLayoutCount    = testLayoutSpan.End    - testLayoutSpan.Begin;

			uint32_t layoutsToTest = std::min(handledLayoutCount, testLayoutCount);
			for(uint32_t layoutIndex = 0; layoutIndex < layoutsToTest; layoutIndex++)
			{
				uint32_t handledLayoutIndex = handledLayoutSpan.Begin + layoutIndex;
				uint32_t testLayoutIndex    = testLayoutSpan.Begin    + layoutIndex;

				if(handledLayoutIndex == testLayoutIndex)
				{
					//These two match
					continue;
				}

				Span<uint32_t> handledLayoutBindingSpan = mSetLayoutRecordNodes[handledLayoutIndex].BindingSpan;
				Span<uint32_t> testLayoutBindingSpan    = mSetLayoutRecordNodes[handledLayoutIndex].BindingSpan;
				if(handledLayoutBindingSpan.Begin == testLayoutBindingSpan.Begin)
				{
					if(handledLayoutBindingSpan.End > testLayoutBindingSpan.End)
					{
						//All bindings of testLayoutBindingSpan are contained in handledLayoutBindingSpan (handled affinity)
						if(affinity == LayoutAffinity::TestAffinity)
						{
							//Can't change the affinity
							mLogger->PostLogMessage("Error trying to merge the layouts for shader groups {" + Utils::ToString(groupNames) + "}: interleaving inclusive layouts\n");
							return Span<uint32_t>
							{
								.Begin = 0,
								.End   = 0
							};
						}

						affinity = LayoutAffinity::HandledAffinity;
						continue;
					}
					else
					{
						//All bindings of handledLayoutBindingSpan are contained in testLayoutBindingSpan (test affinity)
						if(affinity == LayoutAffinity::HandledAffinity)
						{
							//Can't change the affinity
							mLogger->PostLogMessage("Error trying to merge the layouts for shader groups {" + Utils::ToString(groupNames) + "}: interleaving inclusive layouts\n");
							return Span<uint32_t>
							{
								.Begin = 0,
								.End   = 0
							};
						}

						affinity = LayoutAffinity::TestAffinity;
						continue;
					}
				}
				else
				{
					//Try to check the bindings themselves
					uint32_t handledBindingCount = handledLayoutBindingSpan.End - handledLayoutBindingSpan.Begin;
					uint32_t testBingingCount    = testLayoutBindingSpan.End    - testLayoutBindingSpan.Begin;

					uint32_t bindingsToTest = std::min(handledBindingCount, testBingingCount);
					for(uint32_t bindingIndex = 0; bindingIndex < bindingsToTest; bindingIndex++)
					{
						uint32_t handledBindingIndex = handledLayoutBindingSpan.Begin + bindingIndex;
						uint32_t testBindingIndex    = testLayoutBindingSpan.Begin    + bindingIndex;

						if(mLayoutBindingsFlat[handledBindingIndex].binding         != mLayoutBindingsFlat[testBindingIndex].binding
						|| mLayoutBindingsFlat[handledBindingIndex].descriptorType  != mLayoutBindingsFlat[testBindingIndex].descriptorType
						|| mLayoutBindingsFlat[handledBindingIndex].descriptorCount != mLayoutBindingsFlat[testBindingIndex].descriptorCount
						|| mLayoutBindingsFlat[handledBindingIndex].stageFlags      != mLayoutBindingsFlat[testBindingIndex].stageFlags)
						{
							mLogger->PostLogMessage("Error trying to merge the layouts for shader groups {" + Utils::ToString(groupNames) + "}: bindings don't match\n");
							return Span<uint32_t>
							{
								.Begin = 0,
								.End   = 0
							};
						}
					}

					//At this point all the common bindings match
					if(handledBindingCount > testBingingCount)
					{
						//All bindings of testLayoutBindingSpan are contained in handledLayoutBindingSpan (handled affinity)
						if(affinity == LayoutAffinity::TestAffinity)
						{
							//Can't change the affinity
							mLogger->PostLogMessage("Error trying to merge the layouts for shader groups {" + Utils::ToString(groupNames) + "}: interleaving inclusive layouts\n");
							return Span<uint32_t>
							{
								.Begin = 0,
								.End   = 0
							};
						}

						affinity = LayoutAffinity::HandledAffinity;
						continue;
					}
					else
					{
						//All bindings of handledLayoutBindingSpan are contained in testLayoutBindingSpan (test affinity)
						if(affinity == LayoutAffinity::HandledAffinity)
						{
							//Can't change the affinity
							mLogger->PostLogMessage("Error trying to merge the layouts for shader groups {" + Utils::ToString(groupNames) + "}: interleaving inclusive layouts\n");
							return Span<uint32_t>
							{
								.Begin = 0,
								.End   = 0
							};
						}

						affinity = LayoutAffinity::TestAffinity;
						continue;
					}
				}
			}

			if(affinity == LayoutAffinity::TestAffinity)
			{
				handledLayoutSpan = testLayoutSpan;
			}
		}
	}

	return handledLayoutSpan;
}

Span<uint32_t> Vulkan::ShaderDatabase::FindMatchingPushConstantRangeSpan(std::span<std::string_view> groupNames) const
{
	if(groupNames.size() == 0)
	{
		return Span<uint32_t>
		{
			.Begin = 0,
			.End   = 0
		};
	}

	Span<uint32_t> handledRangeSpan = mPushConstantSpansPerShaderGroup.at(groupNames[0]).RangeSpan;
	for(size_t groupIndex = 1; groupIndex < groupNames.size(); groupIndex++)
	{
		std::string_view groupName = groupNames[groupIndex];

		Span<uint32_t> testRangeSpan = mPushConstantSpansPerShaderGroup.at(groupName).RangeSpan;
		if(testRangeSpan.Begin == handledRangeSpan.Begin)
		{
			//The push constant ranges match, just create the minimal span satisfying both
			handledRangeSpan.End = std::max(handledRangeSpan.End, testRangeSpan.End);
		}
		else
		{
			//Try to match the range structs of both spans
			uint32_t handledRangeCount = handledRangeSpan.End - handledRangeSpan.Begin;
			uint32_t testRangeCount    = testRangeSpan.End    - testRangeSpan.Begin;

			uint32_t rangesToTest = std::min(handledRangeCount, testRangeCount);
			for(uint32_t rangeIndex = 0; rangeIndex < rangesToTest; rangeIndex++)
			{
				VkPushConstantRange handledRange = mPushConstantRangesFlat[handledRangeSpan.Begin + rangeIndex];
				VkPushConstantRange testRange    = mPushConstantRangesFlat[testRangeSpan.Begin    + rangeIndex];

				if(handledRange.stageFlags != testRange.stageFlags
				|| handledRange.size       != testRange.size
				|| handledRange.offset     != testRange.offset)
				{
					mLogger->PostLogMessage("Error trying to merge the push constant ranges for shader groups {" + Utils::ToString(groupNames) + "}: ranges don't match\n");
					return Span<uint32_t>
					{
						.Begin = 0,
						.End   = 0
					};
				}
			}

			//At this point all ranges up to rangesToTest match
			if(testRangeCount > handledRangeCount)
			{
				handledRangeSpan = testRangeSpan;
			}
		}
	}

	return handledRangeSpan;
}

uint32_t Vulkan::ShaderDatabase::RegisterSetLayout(uint16_t setDomain, const std::span<VkDescriptorSetLayoutBinding> setBindings, const std::span<BindingRecord> setBindingRecords)
{
	DomainRecord domainRecord = mDomainRecords[setDomain];

	//Traverse the linked list of binding spans for the domain. For each set layout in the domain, try to match the bindings by the type
	uint32_t prevNodeIndex = (uint32_t)(-1);
	uint32_t currNodeIndex = domainRecord.FirstLayoutNodeIndex;
	while(currNodeIndex != (uint32_t)(-1))
	{
		SetLayoutRecordNode currNode = mSetLayoutRecordNodes[currNodeIndex];

		uint32_t layoutBindingCount = currNode.BindingSpan.End - currNode.BindingSpan.Begin;
		if(layoutBindingCount == (uint32_t)setBindings.size())
		{
			//Try to match the set
			uint32_t layoutBindingIndex = 0;
			while(layoutBindingIndex < layoutBindingCount)
			{
				if(mLayoutBindingTypesFlat[(size_t)currNode.BindingSpan.Begin + layoutBindingIndex] != setBindingRecords[layoutBindingIndex].Type)
				{
					//Binding types don't match
					break;
				}

				layoutBindingIndex++;
			}

			if(layoutBindingIndex == layoutBindingCount)
			{
				//All bindings matched
				break;
			}
		}

		prevNodeIndex = currNodeIndex;
		currNodeIndex = currNode.NextNodeIndex;
	}
	
	if(currNodeIndex == (uint32_t)-1)
	{
		//Allocate the space for new bindings
		size_t registeredBindingCount = mLayoutBindingsFlat.size();
		size_t addedBindingCount      = setBindings.size();

		SetLayoutRecordNode newSetBindingNode;
		newSetBindingNode.BindingSpan.Begin = (uint32_t)registeredBindingCount;
		newSetBindingNode.BindingSpan.End   = (uint32_t)(registeredBindingCount + addedBindingCount);
		newSetBindingNode.SetLayout         = nullptr;
		newSetBindingNode.NextNodeIndex     = (uint32_t)-1;

		mSetLayoutRecordNodes.push_back(newSetBindingNode);
		currNodeIndex = (uint32_t)(mSetLayoutRecordNodes.size() - 1);

		if(domainRecord.FirstLayoutNodeIndex == (uint32_t)(-1))
		{
			mDomainRecords[setDomain].FirstLayoutNodeIndex = currNodeIndex;
		}
		else
		{
			mSetLayoutRecordNodes[prevNodeIndex].NextNodeIndex = currNodeIndex;
		}
		
		mLayoutBindingsFlat.resize(registeredBindingCount + addedBindingCount);
		mLayoutBindingFlagsFlat.resize(registeredBindingCount + addedBindingCount);
		mLayoutBindingTypesFlat.resize(registeredBindingCount + addedBindingCount);

		//Validate bindings
		for(uint32_t bindingIndex = newSetBindingNode.BindingSpan.Begin; bindingIndex < newSetBindingNode.BindingSpan.End; bindingIndex++)
		{
			uint32_t layoutBindingIndex = bindingIndex - newSetBindingNode.BindingSpan.Begin;
	
			const VkDescriptorSetLayoutBinding& bindingInfo = setBindings[layoutBindingIndex];

#if defined(DEBUG) || defined(_DEBUG)
			const BindingRecord& bindingRecord = setBindingRecords[layoutBindingIndex];

			assert(ValidateNewBinding(bindingInfo, bindingRecord.DescriptorType, bindingRecord.DescriptorFlags));
			assert(bindingInfo.descriptorCount != (uint32_t)(-1) || bindingInfo.binding == (setBindings.size() - 1)); //Only the last binding can be variable sized
			assert(bindingInfo.binding == layoutBindingIndex);
#endif

			mLayoutBindingsFlat[bindingIndex] = bindingInfo;
		}

		//Initialize binding flags
		for(uint32_t bindingIndex = newSetBindingNode.BindingSpan.Begin; bindingIndex < newSetBindingNode.BindingSpan.End; bindingIndex++)
		{
			uint32_t layoutBindingIndex = bindingIndex - newSetBindingNode.BindingSpan.Begin;

			const BindingRecord& bindingRecord = setBindingRecords[layoutBindingIndex];
			mLayoutBindingFlagsFlat[bindingIndex] = bindingRecord.DescriptorFlags;
		}

		//Initialize binding types
		for(uint32_t bindingIndex = newSetBindingNode.BindingSpan.Begin; bindingIndex < newSetBindingNode.BindingSpan.End; bindingIndex++)
		{
			uint32_t layoutBindingIndex = bindingIndex - newSetBindingNode.BindingSpan.Begin;

			const BindingRecord& bindingRecord = setBindingRecords[layoutBindingIndex];
			mLayoutBindingTypesFlat[bindingIndex] = bindingRecord.Type;
		}
	}
	else
	{
		Span<uint32_t> layoutBindingSpan = mSetLayoutRecordNodes[currNodeIndex].BindingSpan;
		for(uint32_t bindingIndex = 0; bindingIndex < setBindings.size(); bindingIndex++)
		{
			const VkDescriptorSetLayoutBinding& newBindingInfo = setBindings[bindingIndex];
			VkDescriptorSetLayoutBinding& existingBindingInfo  = mLayoutBindingsFlat[(size_t)layoutBindingSpan.Begin + bindingIndex];

			assert(ValidateExistingBinding(newBindingInfo, existingBindingInfo));
			assert(newBindingInfo.binding == bindingIndex);

			existingBindingInfo.stageFlags |= newBindingInfo.stageFlags;
		}
	}

	return currNodeIndex;
}

uint16_t Vulkan::ShaderDatabase::ValidateSetDomain(const std::span<BindingRecord> setBindingRecords)
{
	if(setBindingRecords.size() == 0)
	{
		return UndefinedSetDomain;
	}

	BindingDomain firstDomain = setBindingRecords[0].Domain;
	for(size_t bindingIndex = 1; bindingIndex < setBindingRecords.size(); bindingIndex++)
	{
		BindingRecord bindingRecord = setBindingRecords[bindingIndex];
		if(bindingRecord.Domain != firstDomain)
		{
			//All bindings in the set should belong to the same domain
			return UndefinedSetDomain;
		}
	}

	return firstDomain;
}

void Vulkan::ShaderDatabase::BuildSetLayouts(const DeviceParameters* deviceParameters)
{
	//Fix descriptor count of -1
	for(VkDescriptorSetLayoutBinding& binding: mLayoutBindingsFlat)
	{
		if(binding.descriptorCount == (uint32_t)(-1))
		{
			const VkPhysicalDeviceLimits& limits = deviceParameters->GetDeviceProperties().limits;

			switch(binding.descriptorType)
			{
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
				binding.descriptorCount = limits.maxPerStageDescriptorSampledImages;
				break;
			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
				binding.descriptorCount = limits.maxPerStageDescriptorStorageImages;
				break;
			case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
				binding.descriptorCount = limits.maxPerStageDescriptorUniformBuffers;
				break;
			case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
				binding.descriptorCount = limits.maxPerStageDescriptorStorageBuffers;
				break;
			case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
				binding.descriptorCount = limits.maxPerStageDescriptorInputAttachments;
				break;
			default:
				break;
			}
		}
	}

	//Validate samplers
	uint32_t sharedLayoutRecordIndex = mDomainRecords[SharedSetDomain].FirstLayoutNodeIndex;
	while(sharedLayoutRecordIndex != (uint32_t)(-1))
	{
		const SetLayoutRecordNode& sharedRecord = mSetLayoutRecordNodes[sharedLayoutRecordIndex];

		for(uint32_t bindingIndex = sharedRecord.BindingSpan.Begin; bindingIndex < sharedRecord.BindingSpan.End; bindingIndex++)
		{
			if(mLayoutBindingTypesFlat[bindingIndex] == (uint16_t)SharedDescriptorBindingType::SamplerList)
			{
				mLayoutBindingsFlat[bindingIndex].descriptorCount    = (uint32_t)mSamplerManagerRef->GetSamplerVariableArrayLength();
				mLayoutBindingsFlat[bindingIndex].pImmutableSamplers = mSamplerManagerRef->GetSamplerVariableArray();
			}
		}

		sharedLayoutRecordIndex = sharedRecord.NextNodeIndex;
	}

	for(size_t layoutIndex = 0; layoutIndex < mSetLayoutRecordNodes.size(); layoutIndex++)
	{
		SetLayoutRecordNode& layoutRecordNode   = mSetLayoutRecordNodes[layoutIndex];
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

	mSetLayoutsForCreatePipelineFlat.resize(mLayoutRecordNodeIndicesFlat.size());
	for(uint32_t flatSetLayoutIndex = 0; flatSetLayoutIndex < mLayoutRecordNodeIndicesFlat.size(); flatSetLayoutIndex++)
	{
		mSetLayoutsForCreatePipelineFlat[flatSetLayoutIndex] = mSetLayoutRecordNodes[mLayoutRecordNodeIndicesFlat[flatSetLayoutIndex]].SetLayout;
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

	if(bindingInfo.descriptorCount != (uint32_t)(-1) && (expectedDescriptorFlags & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) != 0)
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