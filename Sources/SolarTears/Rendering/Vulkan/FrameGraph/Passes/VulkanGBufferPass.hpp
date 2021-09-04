#pragma once

#include "../VulkanRenderPass.hpp"
#include "../../../Common/FrameGraph/Passes/GBufferPass.hpp"
#include <span>

namespace Vulkan
{
	class FrameGraphBuilder;
	class DeviceParameters;
	class DescriptorManager;
	class RenderableScene;
	class ShaderDatabase;
	class SharedDescriptorDatabaseBuilder;
	class PassDescriptorDatabaseBuilder;

	class GBufferPass: public RenderPass, public GBufferPassBase
	{
		//IDs for shared pipeline layouts 
		enum class PipelineLayoutType: uint32_t
		{
			Static,
			Rigid,

			Count
		};

	public:
		GBufferPass(VkDevice device, const FrameGraphBuilder* frameGraphBuilder, const std::string& passName, uint32_t frame);
		~GBufferPass();

		void RecordExecution(VkCommandBuffer commandBuffer, const RenderableScene* renderableScene, const FrameGraphConfig& frameGraphConfig) const override;

	public:
		static void RegisterResources(FrameGraphBuilder* frameGraphBuilder, const std::string& passName);
		static void RegisterShaders(ShaderDatabase* shaderDatabase, SharedDescriptorDatabaseBuilder* sharedDescriptorDatabaseBuilder, PassDescriptorDatabaseBuilder* passDescriptorDatabaseBuilder);

	private:
		void CreateRenderPass(const FrameGraphBuilder* frameGraphBuilder, const DeviceParameters* deviceParameters, const std::string& currentPassName);
		void CreateFramebuffer(const FrameGraphBuilder* frameGraphBuilder, const FrameGraphConfig* frameGraphConfig, const std::string& currentPassName, uint32_t frame);
		void CreatePipelineLayout(std::span<VkDescriptorSetLayout> setLayouts, std::span<VkPushConstantRange> pushConstantRanges, VkPipelineLayout* outPipelineLayout);
		void CreateGBufferPipeline(const uint32_t* vertexShaderModule, uint32_t vertexShaderModuleSize, const uint32_t* fragmentShaderModule, uint32_t fragmetShaderModuleSize, VkPipelineLayout pipelineLayout, const FrameGraphConfig* frameGraphConfig, VkPipeline* outPipeline);

	private:
		VkRenderPass  mRenderPass;
		VkFramebuffer mFramebuffer;

		VkPipelineLayout mStaticPipelineLayout;
		VkPipelineLayout mRigidPipelineLayout;

		VkPipeline mStaticPipeline;
		VkPipeline mStaticInstancedPipeline;
		VkPipeline mRigidPipeline;

		std::span<VkDescriptorSet> mSceneCommonAndStaticDescriptorSets; //Common descriptor sets for both pipeline layouts (materials, textures) + descriptor sets for static object drawing (frame data, static object data)
		std::span<VkDescriptorSet> mSceneRigidDescriptorSets;           //Descriptor sets for dynamic objects drawing (frame + dynamic object data)
		uint32_t                   mCommonDescriptorSetCount;           //The number of common descriptor sets

		uint32_t           mMaterialIndexPushConstantOffset;
		VkShaderStageFlags mMaterialIndexPushConstantStages;

		uint32_t           mObjectIndexPushConstantOffset;
		VkShaderStageFlags mObjectIndexPushConstantStages;
	};
}