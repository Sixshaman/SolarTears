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
		SamplerList,    //The variable sized list of all scene samplers. Named "Samplers" in shaders
		TextureList,    //The variable sized list of all scene textures. Named "SceneTextures" in shaders
		MaterialList,   //The variable sized list of all scene materials. Named "SceneMaterials" in shaders
		FrameDataList,  //Frame data. Named "SceneFrameData" in shaders
		ObjectDataList, //The variable sized list of all static object data. Named "SceneObjectDatas" in shaders

		Count,
		Unknown = Count
	};

	static constexpr uint32_t TotalSharedBindings = (uint32_t)(SharedDescriptorBindingType::Count);

	static constexpr std::array<VkDescriptorType, TotalSharedBindings> DescriptorTypesPerSharedBinding = std::to_array
	({
		VK_DESCRIPTOR_TYPE_SAMPLER,
		VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
	});

	constexpr static std::array<std::string_view, TotalSharedBindings> SharedDescriptorBindingNames = std::to_array
	({
		std::string_view("Samplers"),
		std::string_view("SceneTextures"),
		std::string_view("SceneMaterials"),
		std::string_view("SceneFrameData"),
		std::string_view("SceneObjectDatas")
	});

	constexpr static std::array<VkDescriptorBindingFlags, TotalSharedBindings> DescriptorFlagsPerSharedBinding = std::to_array
	({
		(VkDescriptorBindingFlags)0,                                                   //Variable sampler count is not supported, even though samplers are dynamically accessed
		(VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, //Variable texture count
		(VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT, //Variable material count
		(VkDescriptorBindingFlags)0,                                                  //Non-variable frame data count
		(VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT //Variable object count
	});
}