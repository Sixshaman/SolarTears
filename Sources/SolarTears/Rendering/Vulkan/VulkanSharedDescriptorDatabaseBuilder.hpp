#pragma once

#include "VulkanSharedDescriptorDatabase.hpp"
#include "VulkanDescriptorDatabaseCommon.hpp"
#include <string_view>
#include <unordered_map>

namespace Vulkan
{
	class SamplerManager;

	class SharedDescriptorDatabaseBuilder
	{
		struct BindingData
		{
			SharedDescriptorDatabase::SharedDescriptorBindingType BindingType;
			VkDescriptorSetLayoutBinding                          BindingInfo;
		};

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
		SharedDescriptorDatabaseBuilder(SamplerManager* samplerManager);
		~SharedDescriptorDatabaseBuilder();

	public:
		//Tries to register a descriptor set in the database, updating the used shader stage flags for it.
		//Returns SetRegisterResult::Success on success.
		//Returns SetRegisterResult::UndefinedSharedSet if the bindings don't correspond to any sampler or scene data sets.
		//Returns SetRegisterResult::ValidateError if the binding names correspond to sampler or scene data sets, but binding values do not match.
		SetRegisterResult TryRegisterSet(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames);

		void BuildSetLayouts(SharedDescriptorDatabase* databaseToBuild);

	private:
		bool ValidateNewBinding(const VkDescriptorSetLayoutBinding& bindingInfo, SharedDescriptorDatabase::SharedDescriptorBindingType bindingType) const;
		bool ValidateExistingBinding(const VkDescriptorSetLayoutBinding& newBindingInfo, const VkDescriptorSetLayoutBinding& existingBindingInfo) const;

	private:
		const SamplerManager* mSamplerManagerRef;

		std::vector<Span<uint32_t>> mSetBindingSpansPerLayout;
		std::vector<BindingData>    mSetLayoutBindingsFlat;

		std::unordered_map<std::string_view, SharedDescriptorDatabase::SharedDescriptorBindingType> mBindingTypes;
	};
}