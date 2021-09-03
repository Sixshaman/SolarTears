#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <span>
#include <string>
#include <numeric>
#include "../Common/RenderingUtils.hpp"
#include "../../Core/DataStructures/Span.hpp"

namespace Vulkan
{
	class RenderableScene;
	class SamplerManager;

	//The database for all common descriptor sets and set layouts (samplers, scene data)
	class SharedDescriptorDatabase
	{
		friend class SharedDescriptorDatabaseBuilder;

		//Shared descriptor binding type
		enum class SharedDescriptorBindingType: uint32_t
		{
			SamplerList,           //The variable sized list of all scene samplers. Named "Samplers" in shaders
			TextureList,           //The variable sized list of all scene textures. Named "SceneTextures" in shaders
			MaterialList,          //The variable sized list of all scene materials. Named "SceneMaterials" in shaders
			StaticObjectDataList,  //The variable sized list of all static object data. Named "SceneStaticObjectDatas" in shaders
			DynamicObjectDataList, //The variable sized list of all dynamic object data. Named "SceneDynamicObjectDatas" in shaders
			FrameDataList,         //Frame data. Named "SceneFrameData" in shaders

			Count,
			Unknown = Count
		};

		static constexpr uint32_t TotalBindings = (uint32_t)(SharedDescriptorBindingType::Count);

		static constexpr std::array<uint32_t, TotalBindings> FrameCountsPerBinding = std::to_array
		({
			1u,                        //Samplers are static
			1u,                        //Textures are static
			1u,                        //Materials are static
			1u,                        //Static datas are static
			Utils::InFlightFrameCount, //Dynamic datas change from frame to frame
			Utils::InFlightFrameCount  //Frame data changes from frame to frame
		});

		static constexpr std::array<VkDescriptorType, TotalBindings> DescriptorTypesPerBinding = std::to_array
		({
			VK_DESCRIPTOR_TYPE_SAMPLER,
			VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
		});

	public:
		SharedDescriptorDatabase(const VkDevice device);
		~SharedDescriptorDatabase();

		//Creates sets from registered set layouts
		void RecreateSets(const RenderableScene* sceneToCreateDescriptors, const SamplerManager* samplerManager);

	private:
		void RecreateDescriptorPool(const RenderableScene* sceneToCreateDescriptors, const SamplerManager* samplerManager);
		void AllocateDescriptorSets(const RenderableScene* sceneToCreateDescriptors, const SamplerManager* samplerManager);
		void UpdateDescriptorSets(const RenderableScene* sceneToCreateDescriptors);

	private:
		const VkDevice mDeviceRef;

		VkDescriptorPool mDescriptorPool; //оскъ деяйпхорнпнб! оюс! оскъ деяйпхорнпнб! оскъ деяйпхорнпнб! оюс, оюс, оюс! оскъ деяйпхорнпнб!

		std::vector<VkDescriptorSetLayout> mSetLayouts;

		std::vector<Span<uint32_t>>              mSetFormatsPerLayout; //Registered set format spans, <binding count> size each
		std::vector<SharedDescriptorBindingType> mSetFormatsFlat;

		std::vector<Span<uint32_t>>  mSetsPerLayout; //Set spans, <frame count> size each
		std::vector<VkDescriptorSet> mSets;
	};
}