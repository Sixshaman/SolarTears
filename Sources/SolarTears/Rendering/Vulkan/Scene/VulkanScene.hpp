#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include "../VulkanDeviceParameters.hpp"
#include "../../Common/Scene/ModernRenderableScene.hpp"
#include "../../Common/RenderingUtils.hpp"
#include "../../../Core/FrameCounter.hpp"

namespace Vulkan
{
	class RenderableScene: public ModernRenderableScene
	{
		friend class RenderableSceneBuilder;
		friend class SceneDescriptorCreator;

	public:
		RenderableScene(const VkDevice device, const FrameCounter* frameCounter, const DeviceParameters& deviceParameters);
		~RenderableScene();

	public:
		void DrawObjectsOntoGBuffer(VkCommandBuffer commandBuffer, VkPipeline staticPipeline, VkPipeline rigidPipeline) const;

	private:
		const VkDevice mDeviceRef;

		VkBuffer mSceneVertexBuffer;
		VkBuffer mSceneIndexBuffer;

		VkBuffer mSceneUniformBuffer; //Common buffer for all uniform buffer data

		std::vector<VkImage>     mSceneTextures;
		std::vector<VkImageView> mSceneTextureViews;

		VkDescriptorSet mGBufferUniformsDescriptorSets[Utils::InFlightFrameCount];
		VkDescriptorSet mGBufferMaterialDescriptorSet;

		VkDeviceMemory mBufferMemory;
		VkDeviceMemory mBufferHostVisibleMemory;
		VkDeviceMemory mTextureMemory;
	};
}