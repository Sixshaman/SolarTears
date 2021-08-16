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

		void RegisterRequiredSet(std::string_view passName, std::span<VkDescriptorSetLayoutBinding> bindingSpan, std::span<std::string> nameSpan);

	private:
		const VkDevice mDeviceRef;
	

		std::unordered_map<std::string_view, std::vector<std::span<VkDescriptorSetLayoutBinding>>> mSetBindingsPerPass;
		std::unordered_map<std::string_view, std::vector<std::span<std::string>>>                  mSetBindingNamesPerPass;

		std::vector<VkDescriptorSetLayout> mDescriptorSetLayouts;
		std::vector<PassDatabaseBinding>   mDescriptorBindings;
		std::vector<Span<uint32_t>>        mBindingsPerPass;
	};
}