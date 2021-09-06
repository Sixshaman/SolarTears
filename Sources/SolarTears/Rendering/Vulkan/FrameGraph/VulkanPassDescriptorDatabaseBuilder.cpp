#include "VulkanPassDescriptorDatabaseBuilder.hpp"
#include "../VulkanUtils.hpp"
#include "../VulkanFunctions.hpp"
#include <cassert>
#include <VulkanGenericStructures.h>

Vulkan::PassDescriptorDatabaseBuilder::PassDescriptorDatabaseBuilder(const std::vector<PassBindingInfo>& bindingInfos)
{
	mDescriptorTypePerBindingType.resize(bindingInfos.size());
	mDescriptorFlagsPerBindingType.resize(bindingInfos.size());

	for(uint32_t bindingTypeIndex = 0; bindingTypeIndex < (uint32_t)bindingInfos.size(); bindingTypeIndex++)
	{
		mBindingTypeIndices[bindingInfos[bindingTypeIndex].BindingName] = bindingTypeIndex;

		mDescriptorTypePerBindingType[bindingTypeIndex]  = bindingInfos[bindingTypeIndex].BindingDescriptorType;
		mDescriptorFlagsPerBindingType[bindingTypeIndex] = bindingInfos[bindingTypeIndex].BindingFlags;
	}
}

Vulkan::PassDescriptorDatabaseBuilder::~PassDescriptorDatabaseBuilder()
{
}

Vulkan::SetRegisterResult Vulkan::PassDescriptorDatabaseBuilder::TryRegisterSet(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames)
{
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
				auto bindingTypeIt = mBindingTypeIndices.find(bindingNames[layoutBindingIndex]);
				if(bindingTypeIt == mBindingTypeIndices.end())
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
	
			const VkDescriptorSetLayoutBinding& bindingInfo     = setBindings[layoutBindingIndex];
			uint32_t                           bindingTypeIndex = mBindingTypeIndices.at(bindingNames[layoutBindingIndex]);

			if(!ValidateNewBinding(bindingInfo, bindingTypeIndex))
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

			mSetLayoutBindingTypesFlat[layoutBindingIndex] = bindingTypeIndex;
			mSetLayoutBindingsFlat[layoutBindingIndex]     = bindingInfo;
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

uint32_t Vulkan::PassDescriptorDatabaseBuilder::GetSetLayoutCount()
{
	return (uint32_t)mSetBindingSpansPerLayout.size();
}

void Vulkan::PassDescriptorDatabaseBuilder::BuildSetLayouts(VkDevice device, std::span<VkDescriptorSetLayout> setLayoutSpan)
{
	size_t layoutCount       = GetSetLayoutCount();
	size_t totalBindingCount = mSetLayoutBindingsFlat.size();

	assert(layoutCount == (uint32_t)setLayoutSpan.size());

	std::vector<VkDescriptorBindingFlags> bindingFlagsPerBinding(totalBindingCount);
	for(size_t layoutIndex = 0; layoutIndex < layoutCount; layoutIndex++)
	{
		Span<uint32_t> layoutBindingSpan  = mSetBindingSpansPerLayout[layoutIndex];
		uint32_t       layoutBindingCount = layoutBindingSpan.End - layoutBindingSpan.Begin;

		for(uint32_t bindingIndex = layoutBindingSpan.Begin; bindingIndex < layoutBindingSpan.End; bindingIndex++)
		{
			uint32_t bindingTypeIndex = mSetLayoutBindingTypesFlat[bindingIndex];
			bindingFlagsPerBinding[bindingIndex] = mDescriptorFlagsPerBindingType[bindingTypeIndex];
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

		ThrowIfFailed(vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &setLayoutSpan[layoutIndex]));
	}
}

bool Vulkan::PassDescriptorDatabaseBuilder::ValidateNewBinding(const VkDescriptorSetLayoutBinding& bindingInfo, uint32_t bindingType) const
{
	uint32_t bindingTypeIndex = (uint32_t)(bindingType);
	if(bindingInfo.descriptorType != mDescriptorTypePerBindingType[bindingTypeIndex])
	{
		return false;
	}

	if(bindingInfo.descriptorCount == (uint32_t)(-1) && mDescriptorFlagsPerBindingType[bindingTypeIndex] != VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT)
	{
		return false;
	}

	return true;
}

bool Vulkan::PassDescriptorDatabaseBuilder::ValidateExistingBinding(const VkDescriptorSetLayoutBinding& newBindingInfo, const VkDescriptorSetLayoutBinding& existingBindingInfo) const
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