#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string_view>
#include <span>
#include "../../../Core/DataStructures/Span.hpp"

namespace Vulkan
{
	struct PassDatabaseRequest
	{
		uint32_t         SpanSetLayoutIndex;
		uint32_t         Binding;
		std::string_view SubresourceId;
	};

	class PassDescriptorDatabase
	{
		struct PassDatabaseBinding
		{
			uint32_t         SetLayoutIndex;
			uint32_t         Binding;
			std::string_view SubresourceId;
		};

	public:
		PassDescriptorDatabase(const VkDevice device);
		~PassDescriptorDatabase();

		void PostToDatabase(std::span<VkDescriptorSetLayout> descriptorSetLayouts, std::span<PassDatabaseRequest> bindingRequests);

	private:
		const VkDevice mDeviceRef;

		std::vector<VkDescriptorSetLayout> mDescriptorSetLayouts;
		std::vector<PassDatabaseBinding>   mDescriptorBindings;
		std::vector<Span<uint32_t>>        mBindingsPerPass;
	};
}