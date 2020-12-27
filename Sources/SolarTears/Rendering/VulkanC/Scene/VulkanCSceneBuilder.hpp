#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>
#include "VulkanCScene.hpp"
#include "../VulkanCDeviceParameters.hpp"
#include "../VulkanCUtils.hpp"
#include "../../RenderableSceneBuilderBase.hpp"

namespace VulkanCBindings
{
	class MemoryManager;

	class RenderableSceneBuilder: public RenderableSceneBuilderBase
	{
	public:
		RenderableSceneBuilder(RenderableScene* sceneToBuild, uint32_t transferQueueFamilyIndex, uint32_t computeQueueFamilyIndex, uint32_t graphicsQueueFamilyIndex);
		~RenderableSceneBuilder();

	public:
		void BakeSceneFirstPart(const MemoryManager* memoryAllocator, const ShaderManager* shaderManager, const DeviceParameters& deviceParameters);
		void BakeSceneSecondPart(VkCommandBuffer commandBuffer);

	public:
		static constexpr size_t   GetVertexSize();

		static constexpr VkFormat GetVertexPositionFormat();
		static constexpr VkFormat GetVertexNormalFormat();
		static constexpr VkFormat GetVertexTexcoordFormat();

		static constexpr uint32_t GetVertexPositionOffset();
		static constexpr uint32_t GetVertexNormalOffset();
		static constexpr uint32_t GetVertexTexcoordOffset();

	private:
		void CreateSceneMeshMetadata(std::vector<std::wstring>& sceneTexturesVec);

		void CreateSceneDataBuffers(VkDeviceSize* currentIntermediateBufferOffset);
		void LoadTextureImages(const std::vector<std::wstring>& sceneTextures, const DeviceParameters& deviceParameters, VkDeviceSize* currentIntermediateBufferOffset);

		void CreateIntermediateBuffer(const MemoryManager* memoryAllocator, VkDeviceSize intermediateBufferSize);
		void FillIntermediateBufferData();

		void AllocateImageMemory(const MemoryManager* memoryAllocator);
		void AllocateBuffersMemory(const MemoryManager* memoryAllocator);

		void CreateImageViews();

		void CreateDescriptorPool();
		void AllocateDescriptorSets();
		void FillDescriptorSets(const ShaderManager* shaderManager);

	private:
		RenderableScene* mSceneToBuild; 

		uint32_t mGraphicsQueueFamilyIndex;
		uint32_t mComputeQueueFamilyIndex;
		uint32_t mTransferQueueFamilyIndex;

		std::vector<std::vector<VkBufferImageCopy>> mSceneTextureSubresourses;

		std::vector<uint8_t>               mTextureData;
		std::vector<RenderableSceneVertex> mVertexBufferData;
		std::vector<RenderableSceneIndex>  mIndexBufferData;

		VkBuffer       mIntermediateBuffer;
		VkDeviceMemory mIntermediateBufferMemory;

		VkDeviceSize mIntermediateBufferVertexDataOffset;
		VkDeviceSize mIntermediateBufferIndexDataOffset;
		VkDeviceSize mIntermediateBufferTextureDataOffset;
	};
}

#include "VulkanCSceneBuilder.inl"