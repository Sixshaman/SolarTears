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
	class WorkerCommandBuffers;
	class DeviceQueues;

	class RenderableScene: public ModernRenderableScene
	{
		friend class RenderableSceneBuilder;
		friend class SharedDescriptorDatabaseBuilder;

	public:
		RenderableScene(const VkDevice device, const DeviceParameters& deviceParameters);
		~RenderableScene();

	public:
		void        CopyUploadedSceneObjects(WorkerCommandBuffers* commandBuffers, DeviceQueues* deviceQueues, uint32_t frameResourceIndex);
		VkSemaphore GetUploadSemaphore(uint32_t frameResourceIndex) const;

		void PrepareDrawBuffers(VkCommandBuffer commandBuffer) const;

		template<typename SubmeshCallback>
		inline void DrawStaticObjects(VkCommandBuffer commandBuffer, SubmeshCallback submeshCallback) const;

		template<typename MeshCallback, typename SubmeshCallback>
		inline void DrawNonStaticObjects(VkCommandBuffer commandBuffer, MeshCallback meshCallback, SubmeshCallback submeshCallback) const;

	private:
		const VkDevice mDeviceRef;

		VkBuffer mSceneVertexBuffer;
		VkBuffer mSceneIndexBuffer;

		VkBuffer mSceneUniformBuffer; //Common buffer for all uniform buffer data
		VkBuffer mSceneUploadBuffer;  //Common buffer for all data to upload

		std::vector<VkImage>     mSceneTextures;
		std::vector<VkImageView> mSceneTextureViews;

		VkDeviceMemory mBufferMemory;
		VkDeviceMemory mBufferHostVisibleMemory;
		VkDeviceMemory mTextureMemory;

		VkSemaphore               mUploadCopySemaphores[Utils::InFlightFrameCount];
		std::vector<VkBufferCopy> mCurrFrameUploadCopyRegions;
	};

	#include "VulkanScene.inl"
}