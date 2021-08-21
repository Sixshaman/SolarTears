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

	//The database for all common descriptor sets and set layouts (samplers, scene data)
	class SharedDescriptorDatabase
	{
		friend class SharedDescriptorDatabaseBuilder;

		enum class SamplerType : uint32_t
		{
			Linear = 0,
			Anisotropic,

			Count
		};

		static constexpr uint32_t TotalSamplers = (uint32_t)(SamplerType::Count);

		//Scene descriptor sets can only be one of these
		enum class SceneDescriptorSetType: uint32_t
		{
			TextureList,                   //The variable sized list of all scene textures, variable size
			MaterialList,                  //The variable sized list of all scen materials
			StaticObjectDataList,          //The variable sized list of all static object data
			FrameDataList,                 //Frame data
			FrameAndDynamicObjectDataList, //Frame data + the variable sized list of all dynamic object data

			Count,
			Unknown = Count
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

		inline static constexpr uint32_t SetStartIndexPerType(SceneDescriptorSetType type)
		{
			return std::accumulate(SetCountsPerType.begin(), SetCountsPerType.begin() + (uint32_t)type, 0);
		}

	public:
		SharedDescriptorDatabase(const VkDevice device);
		~SharedDescriptorDatabase();

		//Creates sets from registered set layouts
		void RecreateSets(const RenderableScene* sceneToCreateDescriptors);

	private:
		void RecreateDescriptorPool(const RenderableScene* sceneToCreateDescriptors);
		void AllocateSceneSets(const RenderableScene* sceneToCreateDescriptors);
		void UpdateSceneSets(const RenderableScene* sceneToCreateDescriptors);

		void AllocateSamplerSet();

		void CreateSamplers();

	private:
		const VkDevice mDeviceRef;

		VkDescriptorPool mDescriptorPool; //оскъ деяйпхорнпнб! оюс! оскъ деяйпхорнпнб! оскъ деяйпхорнпнб! оюс, оюс, оюс! оскъ деяйпхорнпнб!

		VkDescriptorSetLayout mSamplerDescriptorSetLayout;
		VkDescriptorSet       mSamplerDescriptorSet;

		std::array<VkDescriptorSetLayout, TotalSceneSetLayouts> mSceneSetLayouts;
		std::array<VkDescriptorSet,       TotalSceneSets>       mSceneSets;

		std::array<VkSampler, TotalSamplers> mSamplers;
	};
}