#pragma once

#include <vulkan/vulkan.h>

namespace Vulkan
{
	class ShaderManager;

	class DescriptorManager
	{
	public:
		DescriptorManager(const VkDevice device);
		~DescriptorManager();

	private:
		const VkDevice mDeviceRef;
	};
}