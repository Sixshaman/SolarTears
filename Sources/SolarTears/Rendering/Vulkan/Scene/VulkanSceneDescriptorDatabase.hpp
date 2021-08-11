#pragma once

#include <vulkan/vulkan.h>
#include <span>
#include <unordered_set>

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

	//The request to add a descriptor set layout to the database of scene descriptors
	struct SceneDescriptorDatabaseRequest
	{
		VkDescriptorSetLayout  SetLayout;
		SceneDescriptorSetType SetType;
	};

	class SceneDescriptorDatabase
	{
	public:
		SceneDescriptorDatabase(VkDevice device);
		~SceneDescriptorDatabase();

		//Check if this binding is related to scene objects
		bool IsSceneBinding(std::string_view bindingName);

		SceneDescriptorSetType ComputeSetType(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames);

		void UpdateDatabaseEntries(std::span<SceneDescriptorDatabaseRequest> requests);

		void RecreateDescriptorSets(const RenderableScene* sceneToCreateDescriptors);

	private:
		void RecreateDescriptorPool();

	private:
		VkDevice mDeviceRef;

		VkDescriptorPool mDescriptorPool; //оскъ деяйпхорнпнб! оюс! оскъ деяйпхорнпнб! оскъ деяйпхорнпнб! оюс, оюс, оюс! оскъ деяйпхорнпнб!
	};
}
