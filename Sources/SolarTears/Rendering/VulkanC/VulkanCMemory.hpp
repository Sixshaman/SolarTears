#pragma once

#include <vulkan/vulkan.h>
#include "../../Logging/LoggerQueue.hpp"
#include "VulkanCDeviceParameters.hpp"
#include <vector>

namespace VulkanCBindings
{
	class MemoryManager
	{
	public:
		enum class BufferAllocationType
		{
			DEVICE_LOCAL,
			HOST_VISIBLE
		};

		MemoryManager(LoggerQueue* logger, VkPhysicalDevice physicalDevice, const DeviceParameters& deviceParameters);
		~MemoryManager();

		VkDeviceMemory AllocateImagesMemory(VkDevice device, const std::vector<VkImage>& images, std::vector<VkDeviceSize>& outMemoryOffsets)                                         const;
		VkDeviceMemory AllocateBuffersMemory(VkDevice device, const std::vector<VkBuffer>& buffers, BufferAllocationType allocationType, std::vector<VkDeviceSize>& outMemoryOffsets) const;

		VkDeviceMemory AllocateIntermediateBufferMemory(VkDevice device, VkBuffer buffer) const;

	private:
		LoggerQueue* mLogger;

		VkPhysicalDeviceMemoryProperties          mMemoryProperties;
		VkPhysicalDeviceMemoryBudgetPropertiesEXT mMemoryBudgetProperties;
	};
}