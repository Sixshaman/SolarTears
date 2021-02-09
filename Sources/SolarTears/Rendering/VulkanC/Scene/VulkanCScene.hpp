#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include "../VulkanCDeviceParameters.hpp"
#include "../../RenderableSceneBase.hpp"
#include "../../../Core/FrameCounter.hpp"

namespace VulkanCBindings
{
	class ShaderManager;

	class RenderableScene: public RenderableSceneBase
	{
		friend class RenderableSceneBuilder;

		struct SceneSubobject
		{
			uint32_t IndexCount;
			uint32_t FirstIndex;
			int32_t  VertexOffset;
			uint32_t TextureDescriptorSetIndex;

			//Material data will be set VIA PUSH CONSTANTS
		};

		struct MeshSubobjectRange
		{
			uint32_t FirstSubobjectIndex;
			uint32_t AfterLastSubobjectIndex;
		};

	public:
		RenderableScene(const VkDevice device, const FrameCounter* frameCounter, const DeviceParameters& deviceParameters, const ShaderManager* shaderManager);
		~RenderableScene();

	public:
		void FinalizeSceneUpdating() override;

		void DrawObjectsOntoGBuffer(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) const;


		VkDescriptorSetLayout GetGBufferUniformsDescriptorSetLayout() const;
		VkDescriptorSetLayout GetGBufferTexturesDescriptorSetLayout() const;

	private:
		void CreateSamplers();
		void CreateDescriptorSetLayouts(const ShaderManager* shaderManager);

		VkDeviceSize CalculatePerObjectDataOffset(uint32_t objectIndex, uint32_t currentFrameResourceIndex);
		VkDeviceSize CalculatePerFrameDataOffset(uint32_t currentFrameResourceIndex);

	private:
		//Created from inside

		const VkDevice mDeviceRef;

		const FrameCounter* mFrameCounterRef;

		VkDescriptorSetLayout mGBufferUniformsDescriptorSetLayout;
		VkDescriptorSetLayout mGBufferTexturesDescriptorSetLayout;

		VkSampler mLinearSampler;

		uint32_t mGBufferObjectChunkDataSize;
		uint32_t mGBufferFrameChunkDataSize;

	private:
		//Created from outside

		VkBuffer mSceneVertexBuffer;
		VkBuffer mSceneIndexBuffer;

		VkBuffer mSceneUniformBuffer; //Common buffer for all uniform buffer data

		void* mSceneUniformDataBufferPointer; //Uniform buffer is persistently mapped

		VkDeviceSize mSceneDataUniformObjectBufferOffset;
		VkDeviceSize mSceneDataUniformFrameBufferOffset;

		std::vector<MeshSubobjectRange> mSceneMeshes;
		std::vector<SceneSubobject>     mSceneSubobjects;
		std::vector<VkImage>            mSceneTextures;
		std::vector<VkImageView>        mSceneTextureViews;

		VkDescriptorPool mDescriptorPool;

		VkDescriptorSet              mGBufferUniformsDescriptorSet;
		std::vector<VkDescriptorSet> mGBufferTextureDescriptorSets;

		VkDeviceMemory mBufferMemory;
		VkDeviceMemory mBufferHostVisibleMemory;
		VkDeviceMemory mTextureMemory;
	};
}