#include "VulkanCMemory.hpp"
#include "VulkanCFunctions.hpp"
#include "VulkanCUtils.hpp"
#include <VulkanGenericStructures.h>
#include <cassert>

VulkanCBindings::MemoryManager::MemoryManager(LoggerQueue* logger, VkPhysicalDevice physicalDevice, const DeviceParameters& deviceParameters): mLogger(logger)
{
	vgs::GenericStructureChain<VkPhysicalDeviceMemoryProperties2> memoryPropertiesChain;
	if(deviceParameters.IsMemoryBudgetExtensionEnabled())
	{
		memoryPropertiesChain.AppendToChain(mMemoryBudgetProperties);
	}

	mMemoryBudgetProperties.pNext = nullptr;
	vkGetPhysicalDeviceMemoryProperties2(physicalDevice, &memoryPropertiesChain.GetChainHead());

	mMemoryProperties = memoryPropertiesChain.GetChainHead().memoryProperties;
}

VulkanCBindings::MemoryManager::~MemoryManager()
{
}

VkDeviceMemory VulkanCBindings::MemoryManager::AllocateImagesMemory(VkDevice device, const std::vector<VkImage>& images, std::vector<VkDeviceSize>& outMemoryOffsets) const
{
	outMemoryOffsets.clear();

	VkMemoryRequirements memoryRequirements;
	memoryRequirements.alignment      = 0;
	memoryRequirements.memoryTypeBits = 0xffffffff;
	memoryRequirements.size           = 0;

	VkDeviceSize currMemoryOffset = 0;
	for(size_t i = 0; i < images.size(); i++)
	{
		VkImageMemoryRequirementsInfo2 imageMemoryRequirementsInfo;
		imageMemoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
		imageMemoryRequirementsInfo.pNext = nullptr;
		imageMemoryRequirementsInfo.image = images[i];

		//TODO: dedicated allocation
		VkMemoryRequirements2 imageMemoryRequirements;
		imageMemoryRequirements.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
		imageMemoryRequirements.pNext = nullptr;

		vkGetImageMemoryRequirements2(device, &imageMemoryRequirementsInfo, &imageMemoryRequirements);

		VkDeviceSize memoryAlignment = std::max(memoryRequirements.alignment, imageMemoryRequirements.memoryRequirements.alignment);
		VkDeviceSize memorySize      = VulkanUtils::AlignMemory(imageMemoryRequirements.memoryRequirements.size, memoryAlignment);

		outMemoryOffsets.push_back(currMemoryOffset);
		currMemoryOffset += memorySize;

		memoryRequirements.alignment       = memoryAlignment;
		memoryRequirements.size           += memorySize;
		memoryRequirements.memoryTypeBits &= imageMemoryRequirements.memoryRequirements.memoryTypeBits;
	}

	uint32_t imagesMemoryIndex = (uint32_t)(-1);

	VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	for(uint32_t typeIndex = 0; typeIndex < mMemoryProperties.memoryTypeCount; typeIndex++)
	{
		if((memoryRequirements.memoryTypeBits & (1 << typeIndex)) && (mMemoryProperties.memoryTypes[typeIndex].propertyFlags & memoryFlags) == memoryFlags)
		{
			imagesMemoryIndex = typeIndex;
		}
	}

	assert(imagesMemoryIndex != (uint32_t)(-1));

	VkMemoryAllocateInfo memoryAllocateInfo;
	memoryAllocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.pNext           = nullptr;
	memoryAllocateInfo.allocationSize  = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = imagesMemoryIndex;

	VkDeviceMemory allocatedMemory = nullptr;
	ThrowIfFailed(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &allocatedMemory));

	return allocatedMemory;
}

VkDeviceMemory VulkanCBindings::MemoryManager::AllocateBuffersMemory(VkDevice device, const std::vector<VkBuffer>& buffers, BufferAllocationType allocationType, std::vector<VkDeviceSize>& outMemoryOffsets) const
{
	outMemoryOffsets.clear();

	VkMemoryRequirements memoryRequirements;
	memoryRequirements.alignment      = 0;
	memoryRequirements.memoryTypeBits = 0xffffffff;
	memoryRequirements.size           = 0;

	VkDeviceSize currMemoryOffset = 0;
	for(size_t i = 0; i < buffers.size(); i++)
	{
		VkBufferMemoryRequirementsInfo2 bufferMemoryRequirementsInfo;
		bufferMemoryRequirementsInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2;
		bufferMemoryRequirementsInfo.pNext  = nullptr;
		bufferMemoryRequirementsInfo.buffer = buffers[i];

		//TODO: dedicated allocation
		VkMemoryRequirements2 bufferMemoryRequirements;
		bufferMemoryRequirements.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
		bufferMemoryRequirements.pNext = nullptr;

		vkGetBufferMemoryRequirements2(device, &bufferMemoryRequirementsInfo, &bufferMemoryRequirements);

		VkDeviceSize memoryAlignment = std::max(memoryRequirements.alignment, bufferMemoryRequirements.memoryRequirements.alignment);
		VkDeviceSize memorySize      = VulkanUtils::AlignMemory(bufferMemoryRequirements.memoryRequirements.size, memoryAlignment);

		outMemoryOffsets.push_back(currMemoryOffset);
		currMemoryOffset += memorySize;

		memoryRequirements.alignment       = memoryAlignment;
		memoryRequirements.size           += memorySize;
		memoryRequirements.memoryTypeBits &= bufferMemoryRequirements.memoryRequirements.memoryTypeBits;
	}

	uint32_t bufferMemoryIndex = (uint32_t)(-1);

	VkMemoryPropertyFlags memoryFlags = 0;
	switch (allocationType)
	{
	case BufferAllocationType::DEVICE_LOCAL:
		memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		break;
	case BufferAllocationType::HOST_VISIBLE:
		memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		break;
	default:
		break;
	}

	assert(memoryFlags != 0);
	for(uint32_t typeIndex = 0; typeIndex < mMemoryProperties.memoryTypeCount; typeIndex++)
	{
		if((memoryRequirements.memoryTypeBits & (1 << typeIndex)) && (mMemoryProperties.memoryTypes[typeIndex].propertyFlags & memoryFlags) == memoryFlags)
		{
			bufferMemoryIndex = typeIndex;
		}
	}

	assert(bufferMemoryIndex != (uint32_t)(-1));

	VkMemoryAllocateInfo memoryAllocateInfo;
	memoryAllocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.pNext           = nullptr;
	memoryAllocateInfo.allocationSize  = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = bufferMemoryIndex;

	VkDeviceMemory allocatedMemory = nullptr;
	ThrowIfFailed(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &allocatedMemory));

	return allocatedMemory;
}

VkDeviceMemory VulkanCBindings::MemoryManager::AllocateIntermediateBufferMemory(VkDevice device, VkBuffer buffer) const
{
	VkMemoryRequirements bufferMemoryRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &bufferMemoryRequirements);

	uint32_t intermediateBufferMemoryIndex = (uint32_t)(-1);

	VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	for(uint32_t typeIndex = 0; typeIndex < mMemoryProperties.memoryTypeCount; typeIndex++)
	{
		if((bufferMemoryRequirements.memoryTypeBits & (1 << typeIndex)) && (mMemoryProperties.memoryTypes[typeIndex].propertyFlags & memoryFlags) == memoryFlags)
		{
			intermediateBufferMemoryIndex = typeIndex;
		}
	}

	assert(intermediateBufferMemoryIndex != (uint32_t)(-1));

	vgs::GenericStructureChain<VkMemoryAllocateInfo> memoryAllocateInfoChain;

	//TODO: mGPU?
	VkMemoryAllocateInfo& memoryAllocateInfo = memoryAllocateInfoChain.GetChainHead();
	memoryAllocateInfo.allocationSize        = bufferMemoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex       = intermediateBufferMemoryIndex;

	//The engine is based on Vulkan 1.1, which supports dedicated memory allocation in core. It's better to use dedicated allocation for such things
	VkMemoryDedicatedAllocateInfo dedicatedMemoryAllocationInfo;
	dedicatedMemoryAllocationInfo.pNext  = nullptr;
	dedicatedMemoryAllocationInfo.buffer = buffer;
	dedicatedMemoryAllocationInfo.image  = VK_NULL_HANDLE;

	memoryAllocateInfoChain.AppendToChain(dedicatedMemoryAllocationInfo);

	VkDeviceMemory allocatedMemory = nullptr;
	ThrowIfFailed(vkAllocateMemory(device, &memoryAllocateInfoChain.GetChainHead(), nullptr, &allocatedMemory));

	return allocatedMemory;
}