#include "VulkanSceneDescriptorDatabase.hpp"
#include <unordered_map>
#include <cassert>

bool Vulkan::SceneDescriptorDatabase::IsSceneBinding(std::string_view bindingName)
{
	static const std::unordered_set<std::string_view> sceneBindingNames  = {"SceneTextures", "SceneMaterials", "SceneStaticObjectDatas", "SceneFrameData", "SceneDynamicObjectDatas"};
	return sceneBindingNames.contains(bindingName);
}

Vulkan::SceneDescriptorSetType Vulkan::SceneDescriptorDatabase::ComputeSetType(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames)
{
	if(bindingNames.size() == 1 && bindingNames[0] == "SceneTextures")
	{
		//Texture set bindings
		assert(setBindings[0].binding         == 0);
		assert(setBindings[0].descriptorType  == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
		assert(setBindings[0].descriptorCount == (uint32_t)(-1));

		return SceneDescriptorSetType::TextureList;
	}
	else if(bindingNames.size() == 1 && bindingNames[0] == "SceneMaterials")
	{
		//Material set bindings
		assert(setBindings[0].binding         == 0);
		assert(setBindings[0].descriptorType  == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		assert(setBindings[0].descriptorCount == (uint32_t)(-1));

		return SceneDescriptorSetType::MaterialList;
	}
	else if(bindingNames.size() == 1 && bindingNames[0] == "SceneStaticObjectDatas")
	{
		//Static object data bindings
		assert(setBindings[0].binding         == 0);
		assert(setBindings[0].descriptorType  == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		assert(setBindings[0].descriptorCount == (uint32_t)(-1));

		return SceneDescriptorSetType::StaticObjectDataList;
	}
	else if(bindingNames.size() == 1 && bindingNames[0] == "SceneFrameData")
	{
		//Frame data bindings
		assert(setBindings[0].binding         == 0);
		assert(setBindings[0].descriptorType  == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		assert(setBindings[0].descriptorCount == 1);

		return SceneDescriptorSetType::FrameDataList;
	}
	else if(bindingNames.size() == 2 && bindingNames[0] == "SceneFrameData" && bindingNames[1] == "SceneDynamicObjectDatas")
	{
		//Frame data + dynamic object data
		assert(setBindings[0].binding         == 0);
		assert(setBindings[0].descriptorType  == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		assert(setBindings[0].descriptorCount == 1);

		assert(setBindings[1].binding         == 1);
		assert(setBindings[1].descriptorType  == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		assert(setBindings[1].descriptorCount == (uint32_t)(-1));

		return SceneDescriptorSetType::FrameAndDynamicObjectDataList;
	}

	return SceneDescriptorSetType::Unknown;
}

void Vulkan::SceneDescriptorDatabase::UpdateDatabaseEntries(std::span<SceneDescriptorDatabaseRequest> requests)
{
	
}