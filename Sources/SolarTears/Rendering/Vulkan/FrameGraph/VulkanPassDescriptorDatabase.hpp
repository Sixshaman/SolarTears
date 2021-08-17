#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string_view>
#include <span>
#include <unordered_map>
#include "../../../Core/DataStructures/Span.hpp"

namespace Vulkan
{
	class PassDescriptorDatabase
	{
	public:
		PassDescriptorDatabase(const VkDevice device);
		~PassDescriptorDatabase();

		void RegisterRequiredSet(std::string_view passName, uint32_t setId, VkShaderStageFlags stageFlags);

	private:
		const VkDevice mDeviceRef;

		std::vector<VkShaderStageFlags>                      mShaderStagesPerSets;
		std::unordered_map<std::string_view, Span<uint32_t>> mSetSpansForPasses;
	};
}