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
		RenderableSceneBuilder(RenderableScene* sceneToBuild, MemoryManager* memoryAllocator, DeviceQueues* deviceQueues, WorkerCommandBuffers* workerCommandBuffers, const DeviceParameters* deviceParameters);
		~RenderableSceneBuilder();

	protected:
		void CreateVertexBufferInfo(size_t vertexDataSize)            override final;
		void CreateIndexBufferInfo(size_t indexDataSize)              override final;
		void CreateStaticConstantBufferInfo(size_t constantDataSize)  override final;
		void CreateDynamicConstantBufferInfo(size_t constantDataSize) override final;

		void AllocateTextureMetadataArrays(size_t textureCount)                                                                                                              override final;
		void LoadTextureFromFile(const std::wstring& textureFilename, uint64_t currentIntermediateBufferOffset, size_t textureIndex, std::vector<std::byte>& outTextureData) override final;

		virtual void FinishBufferCreation()  override final;
		virtual void FinishTextureCreation() override final;

		virtual std::byte* MapDynamicConstantBuffer() override final;

		virtual void       CreateIntermediateBuffer()      override final;
		virtual std::byte* MapIntermediateBuffer()   const override final;
		virtual void       UnmapIntermediateBuffer() const override final;

		virtual void WriteInitializationCommands()   const override final;
		virtual void SubmitInitializationCommands()  const override final;
		virtual void WaitForInitializationCommands() const override final;

	private:
		void AllocateImageMemory();
		void AllocateBuffersMemory();

		void CreateImageViews();

	private:
		RenderableScene* mVulkanSceneToBuild;

		MemoryManager*        mMemoryAllocator;
		DeviceQueues*         mDeviceQueues;
		WorkerCommandBuffers* mWorkerCommandBuffers;

		const DeviceParameters* mDeviceParametersRef;

		std::vector<VkImageCreateInfo>     mSceneImageCreateInfos;
		std::vector<VkBufferImageCopy>     mSceneImageCopyInfos;
		std::vector<SubresourceArraySlice> mSceneTextureSubresourceSlices;

		VkBuffer       mIntermediateBuffer;
		VkDeviceMemory mIntermediateBufferMemory;
	};
}