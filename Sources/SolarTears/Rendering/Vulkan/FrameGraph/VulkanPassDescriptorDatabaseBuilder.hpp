#pragma once

#include <vulkan/vulkan.h>
#include <unordered_map>
#include <span>
#include "../../../Core/DataStructures/Span.hpp"

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
		PassDescriptorDatabaseBuilder(uint16_t passDatabaseTypeId, const std::vector<PassBindingInfo>& bindingInfos);
		~PassDescriptorDatabaseBuilder();

	public:
		//Tries to register a descriptor set in the database, updating the used shader stage flags for it. Returns the set id on success and 0xff if no corresponding set was found.
		uint16_t TryRegisterSet(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames);
		uint16_t GetPassTypeId() const;

		uint32_t GetSetLayoutCount();

		void BuildSetLayouts(VkDevice device, std::span<VkDescriptorSetLayout> setLayoutSpan);

	private:
		bool ValidateNewBinding(const VkDescriptorSetLayoutBinding& bindingInfo, uint32_t bindingType)                                            const;
		bool ValidateExistingBinding(const VkDescriptorSetLayoutBinding& newBindingInfo, const VkDescriptorSetLayoutBinding& existingBindingInfo) const;

	private:
		uint16_t mTypeId;

		std::vector<Span<uint32_t>>               mSetBindingSpansPerLayout;
		std::vector<VkDescriptorSetLayoutBinding> mSetLayoutBindingsFlat;
		std::vector<uint32_t>                     mSetLayoutBindingTypesFlat;

		std::unordered_map<std::string_view, uint32_t> mBindingTypeIndices;

		std::vector<VkDescriptorType>         mDescriptorTypePerBindingType;
		std::vector<VkDescriptorBindingFlags> mDescriptorFlagsPerBindingType;
	};
}