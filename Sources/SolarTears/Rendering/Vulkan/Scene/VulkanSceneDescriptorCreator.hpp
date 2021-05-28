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
		SceneDescriptorCreator(RenderableScene* sceneToCreateDescriptors);
		~SceneDescriptorCreator();

		void RecreateSceneDescriptors(const DescriptorManager* descriptorManager, const ShaderManager* shaderManager);

	private:
		void CreateDescriptorPool();
		void AllocateDescriptorSets(const DescriptorManager* descriptorManager);
		void FillDescriptorSets(const ShaderManager* shaderManager);

	private:
		RenderableScene* mSceneToCreateDescriptors;
	};
}