#pragma once

#include "VulkanSharedDescriptorDatabase.hpp"
#include "VulkanDescriptorDatabaseCommon.hpp"

namespace Vulkan
{
	class SharedDescriptorDatabaseBuilder
	{
	public:
		SharedDescriptorDatabaseBuilder();
		~SharedDescriptorDatabaseBuilder();

	public:
		//Tries to register a descriptor set in the database, updating the used shader stage flags for it.
		//Returns SetRegisterResult::Success on success.
		//Returns SetRegisterResult::UndefinedSharedSet if the bindings don't correspond to any sampler or scene data sets.
		//Returns SetRegisterResult::ValidateError if the binding names correspond to sampler or scene data sets, but binding values do not match.
		SetRegisterResult TryRegisterSet(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames);

		void Build(SharedDescriptorDatabase* databaseToBuild);

	private:
		SetRegisterResult TryRegisterSamplerSet(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames);
		SetRegisterResult TryRegisterSceneSet(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames);

		void RecreateSamplerSetLayouts(SharedDescriptorDatabase* databaseToBuild);
		void RecreateSceneSetLayouts(SharedDescriptorDatabase* databaseToBuild);

	private:
		VkShaderStageFlags mSamplerShaderFlags;
		std::array<VkShaderStageFlags, SharedDescriptorDatabase::TotalSceneSetLayouts> mShaderFlagsPerSceneType;
	};
}