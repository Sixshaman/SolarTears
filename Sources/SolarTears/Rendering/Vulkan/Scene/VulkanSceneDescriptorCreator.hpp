#pragma once

#include "VulkanScene.hpp"

namespace Vulkan
{
	class RenderableScene;
	class DescriptorManager;
	class ShaderManager;

	class SceneDescriptorCreator
	{
	public:
		SceneDescriptorCreator();
		~SceneDescriptorCreator();

		void RecreateSceneDescriptors(VkDevice device);

	private:
		void CreateDescriptorPool();
		void AllocateDescriptorSets(const DescriptorManager* descriptorManager);
		void FillDescriptorSets(const ShaderManager* shaderManager);

	private:
		RenderableScene* mSceneToCreateDescriptors;
	};
}