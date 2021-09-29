#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <string_view>
#include "../Common/RenderingUtils.hpp"

namespace Vulkan
{
	//Shared descriptor binding type
	enum class SharedDescriptorBindingType: uint16_t
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

	static constexpr uint32_t TotalSharedBindings = (uint32_t)(SharedDescriptorBindingType::Count);

	static constexpr std::array<uint32_t, TotalSharedBindings> FrameCountsPerSharedBinding = std::to_array
	({
		1u,                        //Samplers are static
		1u,                        //Textures are static
		1u,                        //Materials are static
		1u,                        //Static datas are static
		Utils::InFlightFrameCount, //Dynamic datas change from frame to frame
		Utils::InFlightFrameCount  //Frame data changes from frame to frame
	});

	static constexpr std::array<VkDescriptorType, TotalSharedBindings> DescriptorTypesPerSharedBinding = std::to_array
	({
		VK_DESCRIPTOR_TYPE_SAMPLER,
		VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
	});

	constexpr static std::array<std::string_view, TotalSharedBindings> SharedDescriptorBindingNames = std::to_array
	({
		std::string_view("Samplers"),
		std::string_view("SceneTextures"),
		std::string_view("SceneMaterials"),
		std::string_view("SceneStaticObjectDatas"),
		std::string_view("SceneDynamicObjectDatas"),
		std::string_view("SceneFrameData"),
	});

	constexpr static std::array<VkDescriptorBindingFlags, TotalSharedBindings> DescriptorFlagsPerSharedBinding = std::to_array
	({
		(VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, //Variable sampler count
		(VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, //Variable texture count
		(VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, //Variable material count
		(VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, //Variable static object count
		(VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, //Variable dynamic object count
		(VkDescriptorBindingFlags)0                                                    //Non-variable frame data count
	});
}