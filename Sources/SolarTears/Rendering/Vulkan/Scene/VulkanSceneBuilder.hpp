#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>
#include "VulkanScene.hpp"
#include "../VulkanDeviceParameters.hpp"
#include "../VulkanUtils.hpp"
#include "../../Common/Scene/ModernRenderableSceneBuilder.hpp"

namespace Vulkan
{
	class MemoryManager;
	class DeviceQueues;
	class ShaderManager;
	class DescriptorManager;
	class WorkerCommandBuffers;

	class RenderableSceneBuilder: public ModernRenderableSceneBuilder
	{
	public:
		RenderableSceneBuilder(RenderableScene* sceneToBuild);
		~RenderableSceneBuilder();

	protected:
		uint64_t CreateSceneDataBuffers(const DeviceQueues* deviceQueues, VkDeviceSize currentIntermediateBufferSize);
		uint64_t LoadTextureImages(const std::vector<std::wstring>& sceneTextures, const DeviceParameters& deviceParameters, VkDeviceSize currentIntermediateBufferSize);

		virtual void       CreateIntermediateBuffer(uint64_t intermediateBufferSize) override final;
		virtual std::byte* MapIntermediateBuffer()                                   const override final;
		virtual void       UnmapIntermediateBuffer()                                 const override final;

	private:
		void CreateIntermediateBuffer(const DeviceQueues* deviceQueues, const MemoryManager* memoryAllocator, VkDeviceSize intermediateBufferSize);
		void FillIntermediateBufferData();

		void AllocateImageMemory(const MemoryManager* memoryAllocator);
		void AllocateBuffersMemory(const MemoryManager* memoryAllocator);

		void CreateImageViews();

	private:
		RenderableScene* mVulkanSceneToBuild; 

		std::vector<VkImageCreateInfo>              mSceneTextureCreateInfos;
		std::vector<std::vector<VkBufferImageCopy>> mSceneTextureCopyInfos;

		VkBuffer       mIntermediateBuffer;
		VkDeviceMemory mIntermediateBufferMemory;
	};
}

#include "VulkanSceneBuilder.inl"