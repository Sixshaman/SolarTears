#pragma once

#include <vulkan/vulkan.h>
#include "../../Logging/LoggerQueue.hpp"
#include "VulkanDeviceParameters.hpp"
#include <span>

namespace Vulkan
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

		VkDeviceMemory AllocateImagesMemory(VkDevice device, std::span<VkBindImageMemoryInfo> inoutBindMemoryInfos) const;
		VkDeviceMemory AllocateBuffersMemory(VkDevice device, const std::span<VkBuffer> buffers, BufferAllocationType allocationType, std::vector<VkDeviceSize>& outMemoryOffsets) const;

		VkDeviceMemory AllocateIntermediateBufferMemory(VkDevice device, VkBuffer buffer) const;

	private:
		LoggerQueue* mLogger;

		VkPhysicalDeviceMemoryProperties          mMemoryProperties;
		VkPhysicalDeviceMemoryBudgetPropertiesEXT mMemoryBudgetProperties;
	};
}