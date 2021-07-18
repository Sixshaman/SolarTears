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
		void PrepareDrawBuffers(VkCommandBuffer commandBuffer) const;
		void DrawStaticObjectsWithRootConstants(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VkShaderStageFlags materialIndexShaderFlags, uint32_t materialIndexOffset) const;
		void DrawRigidObjectsWithRootConstants(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VkShaderStageFlags objectIndexShaderFlags, uint32_t objectIndexOffset, VkShaderStageFlags materialIndexShaderFlags, uint32_t materialIndexOffset) const;

	private:
		const VkDevice mDeviceRef;

		VkBuffer mSceneVertexBuffer;
		VkBuffer mSceneIndexBuffer;

		VkBuffer mSceneUniformBuffer; //Common buffer for all uniform buffer data

		std::vector<VkImage>     mSceneTextures;
		std::vector<VkImageView> mSceneTextureViews;

		VkDeviceMemory mBufferMemory;
		VkDeviceMemory mBufferHostVisibleMemory;
		VkDeviceMemory mTextureMemory;
	};
}