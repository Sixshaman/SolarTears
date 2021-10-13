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
}