#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string_view>
#include <span>
#include <unordered_map>
#include <functional>
#include "../../../Core/DataStructures/Span.hpp"

namespace Vulkan
{
	using ValidatePassFunc = std::function<uint32_t(std::span<VkDescriptorSetLayoutBinding>, std::span<std::string>)>;

	class PassDescriptorDatabase
	{
	public:
		PassDescriptorDatabase(const VkDevice device);
		~PassDescriptorDatabase();

		void RegisterRequiredSet(std::string_view passName, uint32_t setId, VkShaderStageFlags stageFlags);

	private:
		const VkDevice mDeviceRef;


	};
}