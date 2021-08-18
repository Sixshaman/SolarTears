#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <span>
#include <string>
#include <numeric>
#include "../Common/RenderingUtils.hpp"

namespace Vulkan
{
	class RenderableScene;

	enum class SetRegisterResult
	{
		Success,
		UndefinedSharedSet,
		ValidateError
	};

	//The database for all common descriptor sets and set layouts (samplers, scene data)
	class SharedDescriptorDatabase
	{
		enum class SamplerType : uint32_t
		{
			Linear = 0,
			Anisotropic,

			Count
		};

		static constexpr uint32_t TotalSamplers = (uint32_t)(SamplerType::Count);

		//Scene descriptor sets can only be one of these
		enum class SceneDescriptorSetType : uint32_t
		{
			TextureList,                   //The variable sized list of all scene textures, variable size
			MaterialList,                  //The variable sized list of all scen materials
			StaticObjectDataList,          //The variable sized list of all static object data
			FrameDataList,                 //Frame data
			FrameAndDynamicObjectDataList, //Frame data + the variable sized list of all dynamic object data

			Count,
			Unknown = Count
		};

		struct SceneSetDatabaseEntry
		{
			VkDescriptorSetLayout      DescriptorSetLayout;
			std::span<VkDescriptorSet> DescriptorSetSpan;
			VkShaderStageFlags         ShaderStageFlags;
		};

		static constexpr uint32_t TotalSceneSetLayouts = (uint32_t)(SceneDescriptorSetType::Count);
		static constexpr std::array<uint32_t, TotalSceneSetLayouts> SetCountsPerType =
		{
			1,                         //Texture set count
			1,                         //Material set count
			1,                         //Static data set count
			Utils::InFlightFrameCount, //Frame data set count
			Utils::InFlightFrameCount  //Frame + dynamic data set count
		};

		static constexpr size_t TotalSceneSets = std::accumulate(SetCountsPerType.begin(), SetCountsPerType.end(), 0);

	public:
		SharedDescriptorDatabase(const VkDevice device);
		~SharedDescriptorDatabase();

		//Tries to register a descriptor set in the database, updating the used shader stage flags for it.
		//Returns SetRegisterResult::Success on success.
		//Returns SetRegisterResult::UndefinedSharedSet if the bindings don't correspond to any sampler or scene data sets.
		//Returns SetRegisterResult::ValidateError if the binding names correspond to sampler or scene data sets, but binding values do not match.
		SetRegisterResult TryRegisterSet(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames);

		//Resets all registered set stage flags and destroys set layouts
		void ResetSetsLayouts();

		//Creates and fills descriptor sets
		void RecreateSetLayouts();

		//Creates sets from registered set layouts
		void RecreateSets(const RenderableScene* sceneToCreateDescriptors);

	private:
		void RecreateSamplerSetLayouts();
		void RecreateSceneSetLayouts();
		void RecreateDescriptorPool(const RenderableScene* sceneToCreateDescriptors);
		void AllocateSets(const RenderableScene* sceneToCreateDescriptors);

		void RecreateDescriptorSets(const RenderableScene* sceneToCreateDescriptors);
		void UpdateDescriptorSets(const RenderableScene* sceneToCreateDescriptors);

		void CreateSamplers();

	private:
		const VkDevice mDeviceRef;

		VkDescriptorPool mDescriptorPool; //оскъ деяйпхорнпнб! оюс! оскъ деяйпхорнпнб! оскъ деяйпхорнпнб! оюс, оюс, оюс! оскъ деяйпхорнпнб!

		VkDescriptorSetLayout mSamplerDescriptorSetLayout;
		VkDescriptorSet       mSamplerDescriptorSet;
		VkShaderStageFlags    mSamplerShaderFlags;

		std::array<SceneSetDatabaseEntry, TotalSceneSetLayouts> mSceneEntriesPerType;
		std::array<VkDescriptorSet,       TotalSceneSets>       mSceneSets;

		std::array<VkSampler, TotalSamplers> mSamplers;
	};
}