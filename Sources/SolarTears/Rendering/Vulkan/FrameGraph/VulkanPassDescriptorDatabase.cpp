#include "VulkanPassDescriptorDatabase.hpp"
#include "../VulkanUtils.hpp"
#include "../VulkanFunctions.hpp"
#include <algorithm>

Vulkan::PassDescriptorDatabase::PassDescriptorDatabase(const VkDevice device): mDeviceRef(device)
{
}

Vulkan::PassDescriptorDatabase::~PassDescriptorDatabase()
{
	for(VkDescriptorSetLayout& layoutToDestroy: mDescriptorSetLayouts)
	{
		SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, layoutToDestroy);
	}
}

void Vulkan::PassDescriptorDatabase::RegisterRequiredSet(std::string_view passName, uint32_t setId, VkShaderStageFlags stageFlags)
{
	auto passIt = mSetSpansForPasses.find(passName);
	if(passIt == mSetSpansForPasses.end())
	{
		Span<uint32_t> passSetIds;
		passSetIds.Begin = (uint32_t)mShaderStagesPerSets.size();
		passSetIds.End   = (uint32_t)mShaderStagesPerSets.size() + setId + 1;

		mShaderStagesPerSets.resize(passSetIds.End, 0);
		mShaderStagesPerSets[passSetIds.Begin + setId] |= stageFlags;

		mSetSpansForPasses[passName] = passSetIds;
	}
	else
	{
		Span<uint32_t>& passSetIds = passIt->second;
		if(passSetIds.Begin + setId < passSetIds.End)
		{
			//Just update the corresponding stage flags
			mShaderStagesPerSets[passSetIds.Begin + setId] |= stageFlags;
		}
		else
		{
			if(passSetIds.End == mShaderStagesPerSets.size())
			{
				//Can simply append new ids to the end
				passSetIds.End = passSetIds.Begin + setId + 1;

				mShaderStagesPerSets.resize(passSetIds.End, 0);
				mShaderStagesPerSets[passSetIds.Begin + setId] |= stageFlags;
			}
			else
			{
				//Need to swap this span and all spans after it (expensive)
				uint32_t spanStart = passSetIds.Begin;
				uint32_t addedSize = (setId + 1) - (passSetIds.End - passSetIds.Begin);

				std::vector<VkShaderStageFlags> newSetFlags;
				newSetFlags.reserve(mSetSpansForPasses.size() + addedSize - spanStart);

				for(auto it = mSetSpansForPasses.begin(); it != mSetSpansForPasses.end(); ++it)
				{
					Span<uint32_t> oldSpan = it->second;
					if(oldSpan.Begin < spanStart)
					{
						//No need in sorting this part
						continue;
					}

					if(it->first == passName)
					{
						//Put to the end
						continue;
					}

					Span<uint32_t> movedSpan;
					movedSpan.Begin = spanStart + (uint32_t)newSetFlags.size();
					movedSpan.End   = spanStart + (uint32_t)newSetFlags.size() + (oldSpan.Begin - oldSpan.End);

					for(uint32_t setIndex = oldSpan.Begin; setIndex < oldSpan.End; setIndex++)
					{
						newSetFlags.push_back(mShaderStagesPerSets[setIndex]);
					}

					it->second = movedSpan;
				}

				Span<uint32_t> movedSpanForPass;
				passSetIds.Begin = spanStart + (uint32_t)newSetFlags.size();
				passSetIds.End   = passSetIds.Begin + setId + 1;

				for(uint32_t setIndex = passSetIds.Begin; setIndex < passSetIds.End; setIndex++)
				{
					newSetFlags.push_back(mShaderStagesPerSets[setIndex]);
				}

				newSetFlags.resize(newSetFlags.size() + addedSize, 0);
				passIt->second = movedSpanForPass;

				mShaderStagesPerSets.insert(mShaderStagesPerSets.begin() + spanStart, newSetFlags.begin(), newSetFlags.end());
			}	
		}
	}
}