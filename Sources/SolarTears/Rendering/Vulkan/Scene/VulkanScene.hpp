#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include "../VulkanDeviceParameters.hpp"
#include "../../Common/Scene/ModernRenderableScene.hpp"
#include "../../Common/RenderingUtils.hpp"
#include "../../../Core/FrameCounter.hpp"
#include "../VulkanFunctions.hpp"

namespace Vulkan
{
	class RenderableScene: public ModernRenderableScene
	{
		friend class RenderableSceneBuilder;
		friend class SharedDescriptorDatabaseBuilder;

	public:
		RenderableScene(const VkDevice device, const DeviceParameters& deviceParameters);
		~RenderableScene();

	public:
		void PrepareDrawBuffers(VkCommandBuffer commandBuffer) const;

		template<typename SubmeshCallback>
		inline void DrawStaticObjects(VkCommandBuffer commandBuffer, SubmeshCallback submeshCallback) const;

		template<typename MeshCallback, typename SubmeshCallback>
		inline void DrawNonStaticObjects(VkCommandBuffer commandBuffer, MeshCallback meshCallback, SubmeshCallback submeshCallback) const;

	private:
		const VkDevice mDeviceRef;

		VkBuffer mSceneVertexBuffer;
		VkBuffer mSceneIndexBuffer;

		VkBuffer mSceneStaticUniformBuffer; //Common buffer for all uniform buffer data
		VkBuffer mSceneDynamicDataBuffer;   //Common buffer for all dynamic data

		std::vector<VkImage>     mSceneTextures;
		std::vector<VkImageView> mSceneTextureViews;

		VkDeviceMemory mBufferMemory;
		VkDeviceMemory mBufferHostVisibleMemory;
		VkDeviceMemory mTextureMemory;
	};

	#include "VulkanScene.inl"
}