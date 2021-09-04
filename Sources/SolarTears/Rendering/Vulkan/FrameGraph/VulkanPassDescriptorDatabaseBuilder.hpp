#pragma once

#include <vulkan/vulkan.h>
#include <unordered_map>
#include <span>
#include "../../../Core/DataStructures/Span.hpp"
#include "../VulkanDescriptorDatabaseCommon.hpp"

namespace Vulkan
{
	class PassDescriptorDatabaseBuilder
	{
	public:
		struct PassBindingInfo
		{
			std::string_view         BindingName;
			VkDescriptorType         BindingDescriptorType;
			VkDescriptorBindingFlags BindingFlags;
		};

	public:
		PassDescriptorDatabaseBuilder(VkDevice device, const std::vector<PassBindingInfo>& bindingInfos);
		~PassDescriptorDatabaseBuilder();

	public:
		//Tries to register a descriptor set in the database, updating the used shader stage flags for it.
		//Returns SetRegisterResult::Success on success.
		//Returns SetRegisterResult::UndefinedSharedSet if the bindings don't correspond to any sampler or scene data sets.
		//Returns SetRegisterResult::ValidateError if the binding names correspond to sampler or scene data sets, but binding values do not match.
		SetRegisterResult TryRegisterSet(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames);

		void BuildSetLayouts();

	private:
		bool ValidateNewBinding(const VkDescriptorSetLayoutBinding& bindingInfo, uint32_t bindingType)                                            const;
		bool ValidateExistingBinding(const VkDescriptorSetLayoutBinding& newBindingInfo, const VkDescriptorSetLayoutBinding& existingBindingInfo) const;

	private:
		const VkDevice mDeviceRef;

		std::vector<Span<uint32_t>>               mSetBindingSpansPerLayout;
		std::vector<VkDescriptorSetLayoutBinding> mSetLayoutBindingsFlat;
		std::vector<uint32_t>                     mSetLayoutBindingTypesFlat;

		std::unordered_map<std::string_view, uint32_t> mBindingTypeIndices;

		std::vector<VkDescriptorType>         mDescriptorTypePerBindingType;
		std::vector<VkDescriptorBindingFlags> mDescriptorFlagsPerBindingType;
	};
}