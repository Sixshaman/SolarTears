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
	class SceneDescriptorDatabase;

	class GBufferPass: public RenderPass, public GBufferPassBase
	{
	public:
		GBufferPass(VkDevice device, const FrameGraphBuilder* frameGraphBuilder, const std::string& passName, uint32_t frame);
		~GBufferPass();

		void RecordExecution(VkCommandBuffer commandBuffer, const RenderableScene* renderableScene, const FrameGraphConfig& frameGraphConfig) const override;

	public:
		static void OnAdd(FrameGraphBuilder* frameGraphBuilder, const std::string& passName);

	private:
		static void RegisterDescriptorSets(std::span<std::wstring> shaderModuleNames, const FrameGraphBuilder* frameGraphBuilder);

		void CreateRenderPass(const FrameGraphBuilder* frameGraphBuilder, const DeviceParameters* deviceParameters, const std::string& currentPassName);
		void CreateFramebuffer(const FrameGraphBuilder* frameGraphBuilder, const FrameGraphConfig* frameGraphConfig, const std::string& currentPassName, uint32_t frame);
		void CreateGBufferPipeline(const uint32_t* vertexShaderModule, uint32_t vertexShaderModuleSize, const uint32_t* fragmentShaderModule, uint32_t fragmetShaderModuleSize, VkPipelineLayout pipelineLayout, FrameGraphConfig* frameGraphConfig, VkPipeline* outPipeline);

	private:
		VkRenderPass  mRenderPass;
		VkFramebuffer mFramebuffer;

		VkPipelineLayout mStaticPipelineLayout;
		VkPipelineLayout mRigidPipelineLayout;

		VkPipeline mStaticPipeline;
		VkPipeline mStaticInstancedPipeline;
		VkPipeline mRigidPipeline;

		std::span<VkDescriptorSet> mSceneCommonAndStaticDescriptorSets; //All descriptor sets for static object drawing (materials, textures, frame + object data)
		std::span<VkDescriptorSet> mSceneRigidDescriptorSets;           //Just the object data descriptor sets for dynamic objects (frame + dynamic object data)
		uint32_t                   mSceneObjectDataSetNumber;           //The descriptor set for object data

		uint32_t           mMaterialIndexPushConstantOffset;
		VkShaderStageFlags mMaterialIndexPushConstantStages;

		uint32_t           mObjectIndexPushConstantOffset;
		VkShaderStageFlags mObjectIndexPushConstantStages;
	};
}