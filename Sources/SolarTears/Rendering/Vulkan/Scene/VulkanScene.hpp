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
		void DrawBakedPositionObjectsWithPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VkShaderStageFlags materialIndexShaderFlags, uint32_t materialIndexOffset) const;
		void DrawBufferedPositionObjectsWithPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VkShaderStageFlags objectIndexShaderFlags, uint32_t objectIndexOffset, VkShaderStageFlags materialIndexShaderFlags, uint32_t materialIndexOffset) const;

	private:
		const VkDevice mDeviceRef;

		VkBuffer mSceneVertexBuffer;
		VkBuffer mSceneIndexBuffer;

		VkBuffer mSceneStaticUniformBuffer;  //Common buffer for all static uniform buffer data
		VkBuffer mSceneDynamicUniformBuffer; //Common buffer for all dynamic uniform buffer data

		std::vector<VkImage>     mSceneTextures;
		std::vector<VkImageView> mSceneTextureViews;

		VkDeviceMemory mBufferMemory;
		VkDeviceMemory mBufferHostVisibleMemory;
		VkDeviceMemory mTextureMemory;
	};
}