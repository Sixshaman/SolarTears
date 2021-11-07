#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>
#include "VulkanScene.hpp"
#include "../VulkanDeviceParameters.hpp"
#include "../VulkanUtils.hpp"
#include "../../Common/Scene/ModernRenderableSceneBuilder.hpp"
#include "../../../Core/DataStructures/Span.hpp"

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
		RenderableSceneBuilder(RenderableScene* sceneToBuild, MemoryManager* memoryAllocator, DeviceQueues* deviceQueues, WorkerCommandBuffers* workerCommandBuffers, const DeviceParameters* deviceParameters);
		~RenderableSceneBuilder();

	protected:
		void CreateVertexBufferInfo(size_t vertexDataSize)     override final;
		void CreateIndexBufferInfo(size_t indexDataSize)       override final;
		void CreateConstantBufferInfo(size_t constantDataSize) override final;
		void CreateUploadBufferInfo(size_t uploadDataSize)     override final;

		void AllocateTextureMetadataArrays(size_t textureCount)                                                                                                              override final;
		void LoadTextureFromFile(const std::wstring& textureFilename, uint64_t currentIntermediateBufferOffset, size_t textureIndex, std::vector<std::byte>& outTextureData) override final;

		virtual void FinishBufferCreation()  override final;
		virtual void FinishTextureCreation() override final;

		virtual std::byte* MapUploadBuffer() override final;

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

		std::vector<VkFormat>          mSceneImageFormats;
		std::vector<VkBufferImageCopy> mSceneImageCopyInfos;
		std::vector<Span<uint32_t>>    mSceneTextureSubresourceSpans;

		VkBuffer       mIntermediateBuffer;
		VkDeviceMemory mIntermediateBufferMemory;
	};
}