#include "VulkanDescriptors.hpp"
#include "VulkanUtils.hpp"
#include "VulkanFunctions.hpp"
#include "Scene/VulkanScene.hpp"
#include <VulkanGenericStructures.h>

Vulkan::DescriptorDatabase::DescriptorDatabase(const VkDevice device): mDeviceRef(device)
{
	mSharedDescriptorPool = VK_NULL_HANDLE;
	mPassDescriptorPool   = VK_NULL_HANDLE;
}

Vulkan::DescriptorDatabase::~DescriptorDatabase()
{
	ClearDatabase();

	SafeDestroyObject(vkDestroyDescriptorPool, mDeviceRef, mSharedDescriptorPool);
	SafeDestroyObject(vkDestroyDescriptorPool, mDeviceRef, mPassDescriptorPool);
}

void Vulkan::DescriptorDatabase::ClearDatabase()
{
	SafeDestroyObject(vkDestroyDescriptorPool, mDeviceRef, mSharedDescriptorPool);
	for(size_t entryIndex = 0; entryIndex < mSharedSetCreateMetadatas.size(); entryIndex++)
	{
		if(mSharedSetCreateMetadatas[entryIndex].SetFrameIndex == 0)
		{
			SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, mSharedSetCreateMetadatas[entryIndex].SetLayout);
		}
		else
		{
			mSharedSetCreateMetadatas[entryIndex].SetLayout = VK_NULL_HANDLE;
		}
	}

	mSharedSetCreateMetadatas.clear();
	mSharedSetFormatsFlat.clear();
	mSharedSetRecords.clear();
}

std::span<VkDescriptorSet> Vulkan::DescriptorDatabase::ValidateSetSpan(std::span<VkDescriptorSet> setToValidate, const VkDescriptorSet* originalSpanStartPoint)
{
	ptrdiff_t spanStart = setToValidate.data() - originalSpanStartPoint;
	size_t    spanSize  = setToValidate.size();

	return std::span<VkDescriptorSet>{mDescriptorSets.begin() + spanStart, mDescriptorSets.begin() + spanStart + spanSize};
}
