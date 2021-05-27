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
		struct SubresourceArraySlice
		{
			uint32_t Begin;
			uint32_t End;
		};

	public:
		RenderableSceneBuilder(RenderableScene* sceneToBuild, const DeviceQueues* deviceQueues, const DeviceParameters* deviceParameters);
		~RenderableSceneBuilder();

	protected:
		void PreCreateVertexBuffer(size_t vertexDataSize)                        override final;
		void PreCreateIndexBuffer(size_t indexDataSize)                          override final;
		void PreCreateConstantBuffer(size_t constantDataSize)                    override final;

		void AllocateTextureMetadataArrays(size_t textureCount)                                                                                                              override final;
		void LoadTextureFromFile(const std::wstring& textureFilename, uint64_t currentIntermediateBufferOffset, size_t textureIndex, std::vector<std::byte>& outTextureData) override final;

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

		const DeviceQueues*     mDeviceQueuesRef;
		const DeviceParameters* mDeviceParametersRef;

		std::vector<VkImageCreateInfo>     mSceneImageCreateInfos;
		std::vector<VkBufferImageCopy>     mSceneImageCopyInfos;
		std::vector<SubresourceArraySlice> mSceneTextureSubresourceSlices;

		VkBuffer       mIntermediateBuffer;
		VkDeviceMemory mIntermediateBufferMemory;
	};
}

#include "VulkanSceneBuilder.inl"