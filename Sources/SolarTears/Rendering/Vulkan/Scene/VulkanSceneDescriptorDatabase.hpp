#pragma once

#include <vulkan/vulkan.h>
#include <span>
#include <array>
#include <unordered_set>
#include "../../../Core/DataStructures/Span.hpp"

namespace Vulkan
{
	class RenderableScene;

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

	class SceneDescriptorDatabase
	{
		struct DatabaseEntry
		{
			VkDescriptorSetLayout DescriptorSetLayout;
			Span<uint32_t>        AllocatedSetSpan;
			VkShaderStageFlags    ShaderStageFlags;
		};

	public:
		SceneDescriptorDatabase(VkDevice device);
		~SceneDescriptorDatabase();

		//Check if this set is of scene objects
		SceneDescriptorSetType ComputeSetType(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames);

		void ResetRegisteredSets();
		void RegisterRequiredSet(SceneDescriptorSetType setType, VkShaderStageFlags setShaderFlags);

		void RecreateDescriptorSets(const RenderableScene* sceneToCreateDescriptors);
		void UpdateDescriptorSets(const RenderableScene* sceneToCreateDescriptors);

	private:
		void RecreateSetLayouts();
		void AllocateSpaceForSets();
		void RecreateDescriptorPool(const RenderableScene* sceneToCreateDescriptors);
		void AllocateSets(const RenderableScene* sceneToCreateDescriptors);

	private:
		VkDevice mDeviceRef;

		std::vector<VkDescriptorSet> mAllocatedSets;

		VkDescriptorPool mDescriptorPool; //оскъ деяйпхорнпнб! оюс! оскъ деяйпхорнпнб! оскъ деяйпхорнпнб! оюс, оюс, оюс! оскъ деяйпхорнпнб!

		std::array<DatabaseEntry, (size_t)SceneDescriptorSetType::Count> mEntriesPerType;
	};
}
