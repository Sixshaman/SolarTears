#pragma once

#include "VulkanSharedDescriptorDatabase.hpp"
#include <string_view>
#include <unordered_map>

namespace Vulkan
{
	class SamplerManager;

	class SharedDescriptorDatabaseBuilder
	{
		constexpr static std::array<std::string_view, SharedDescriptorDatabase::TotalBindings> DescriptorBindingNames = std::to_array
		({
			std::string_view("Samplers"),
			std::string_view("SceneTextures"),
			std::string_view("SceneMaterials"),
			std::string_view("SceneStaticObjectDatas"),
			std::string_view("SceneDynamicObjectDatas"),
			std::string_view("SceneFrameData"),
		});

		constexpr static std::array<VkDescriptorBindingFlags, SharedDescriptorDatabase::TotalBindings> DescriptorFlagsPerBinding = std::to_array
		({
			(VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, //Variable sampler count
			(VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, //Variable texture count
			(VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, //Variable material count
			(VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, //Variable static object count
			(VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, //Variable dynamic object count
			(VkDescriptorBindingFlags)0                                                    //Non-variable frame data count
		});

	public:
		SharedDescriptorDatabaseBuilder(VkDevice device, SamplerManager* samplerManager);
		~SharedDescriptorDatabaseBuilder();

	public:
		//Tries to register a descriptor set in the database, updating the used shader stage flags for it. Returns the set id on success and 0xff if no corresponding set was found.
		uint16_t TryRegisterSet(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames);

		void BuildSetLayouts();

		void FlushSetLayoutInfos(SharedDescriptorDatabase* databaseToBuild);

	private:
		bool ValidateNewBinding(const VkDescriptorSetLayoutBinding& bindingInfo, SharedDescriptorDatabase::SharedDescriptorBindingType bindingType) const;
		bool ValidateExistingBinding(const VkDescriptorSetLayoutBinding& newBindingInfo, const VkDescriptorSetLayoutBinding& existingBindingInfo) const;

	private:
		const VkDevice mDeviceRef;

		const SamplerManager* mSamplerManagerRef;

		std::vector<Span<uint32_t>>                                        mSetBindingSpansPerLayout;
		std::vector<VkDescriptorSetLayoutBinding>                          mSetLayoutBindingsFlat;
		std::vector<SharedDescriptorDatabase::SharedDescriptorBindingType> mSetLayoutBindingTypesFlat;

		std::vector<VkDescriptorSetLayout> mSetLayouts;

		std::unordered_map<std::string_view, SharedDescriptorDatabase::SharedDescriptorBindingType> mBindingTypes;
	};
}