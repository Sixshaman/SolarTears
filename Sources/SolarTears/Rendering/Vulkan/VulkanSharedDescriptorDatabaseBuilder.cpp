#include "VulkanSharedDescriptorDatabaseBuilder.hpp"
#include "VulkanFunctions.hpp"
#include "VulkanUtils.hpp"
#include "VulkanSamplers.hpp"
#include <VulkanGenericStructures.h>

Vulkan::SharedDescriptorDatabaseBuilder::SharedDescriptorDatabaseBuilder(VkDevice device, SamplerManager* samplerManager): mDeviceRef(device), mSamplerManagerRef(samplerManager)
{
	for(uint32_t bindingTypeIndex = 0; bindingTypeIndex < SharedDescriptorDatabase::TotalBindings; bindingTypeIndex++)
	{
		std::string_view bindingName = DescriptorBindingNames[bindingTypeIndex];
		mBindingTypes[bindingName] = (SharedDescriptorDatabase::SharedDescriptorBindingType)bindingTypeIndex;
	}
}

Vulkan::SharedDescriptorDatabaseBuilder::~SharedDescriptorDatabaseBuilder()
{
	//Destroy the non-flushed layouts
	for(VkDescriptorSetLayout setLayout: mSetLayouts)
	{
		SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, setLayout);
	}
}

Vulkan::SetRegisterResult Vulkan::SharedDescriptorDatabaseBuilder::TryRegisterSet(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames)
{
	if(bindingNames.size() == 0)
	{
		//Assume empty set is invalid
		return SetRegisterResult::ValidateError;
	}

	if(!mBindingTypes.contains(bindingNames[0]))
	{
		//Assume that either all bindings are shared or none
		return SetRegisterResult::UndefinedSet;
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
					return SetRegisterResult::ValidateError;
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

			if(!ValidateNewBinding(bindingInfo, bindingType))
			{
				return SetRegisterResult::ValidateError;
			}

			if(bindingInfo.descriptorCount == (uint32_t)(-1) && bindingInfo.binding != (setBindings.size() - 1)) //Only the last binding can be variable sized
			{
				return SetRegisterResult::ValidateError;
			}

			if(bindingInfo.binding != layoutBindingIndex)
			{
				return SetRegisterResult::ValidateError;
			}

			mSetLayoutBindingTypesFlat[layoutBindingIndex] = bindingType;
			mSetLayoutBindingsFlat[layoutBindingIndex]     = bindingInfo;

			if(bindingType == SharedDescriptorDatabase::SharedDescriptorBindingType::SamplerList)
			{
				mSetLayoutBindingsFlat[layoutBindingIndex].pImmutableSamplers = mSamplerManagerRef->GetSamplerVariableArray();
			}
		}
	}
	else
	{
		Span<uint32_t> layoutBindingSpan = mSetBindingSpansPerLayout[matchingLayoutIndex];

		VkShaderStageFlags setStageFlags;
		for(uint32_t bindingIndex = 0; bindingIndex < setBindings.size(); bindingIndex++)
		{
			const VkDescriptorSetLayoutBinding& newBindingInfo = setBindings[bindingIndex];
			VkDescriptorSetLayoutBinding& existingBindingInfo  = mSetLayoutBindingsFlat[(size_t)layoutBindingSpan.Begin + bindingIndex];

			if(!ValidateExistingBinding(newBindingInfo, existingBindingInfo))
			{
				return SetRegisterResult::ValidateError;
			}

			if(newBindingInfo.binding != bindingIndex)
			{
				return SetRegisterResult::ValidateError;
			}

			existingBindingInfo.stageFlags |= newBindingInfo.stageFlags;
		}
	}

	return SetRegisterResult::Success;
}

void Vulkan::SharedDescriptorDatabaseBuilder::BuildSetLayouts()
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
			uint32_t layoutBindingIndex = bindingIndex - layoutBindingSpan.Begin;
			bindingFlagsPerBinding[bindingIndex] = DescriptorFlagsPerBinding[layoutBindingIndex];
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

void Vulkan::SharedDescriptorDatabaseBuilder::FlushSetLayoutInfos(SharedDescriptorDatabase* databaseToBuild)
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

bool Vulkan::SharedDescriptorDatabaseBuilder::ValidateNewBinding(const VkDescriptorSetLayoutBinding& bindingInfo, SharedDescriptorDatabase::SharedDescriptorBindingType bindingType) const
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

bool Vulkan::SharedDescriptorDatabaseBuilder::ValidateExistingBinding(const VkDescriptorSetLayoutBinding& newBindingInfo, const VkDescriptorSetLayoutBinding& existingBindingInfo) const
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