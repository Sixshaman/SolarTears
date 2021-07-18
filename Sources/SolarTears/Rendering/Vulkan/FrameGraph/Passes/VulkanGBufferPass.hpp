#pragma once

#include "../VulkanRenderPass.hpp"
#include "../../../Common/FrameGraph/Passes/GBufferPass.hpp"
#include "../../../3rdParty/SPIRV-Reflect/spirv_reflect.h"

namespace Vulkan
{
	class FrameGraphBuilder;
	class DeviceParameters;
	class DescriptorManager;
	class RenderableScene;

	class GBufferPass: public RenderPass, public GBufferPassBase
	{
	public:
		GBufferPass(VkDevice device, const FrameGraphBuilder* frameGraphBuilder, const std::string& passName, uint32_t frame);
		~GBufferPass();

		void RecordExecution(VkCommandBuffer commandBuffer, const RenderableScene* renderableScene, const FrameGraphConfig& frameGraphConfig) const override;

	public:
		static void OnAdd(FrameGraphBuilder* frameGraphBuilder, const std::string& passName);

	private:
		void CreateRenderPass(const FrameGraphBuilder* frameGraphBuilder, const DeviceParameters* deviceParameters, const std::string& currentPassName);
		void CreateFramebuffer(const FrameGraphBuilder* frameGraphBuilder, const FrameGraphConfig* frameGraphConfig, const std::string& currentPassName, uint32_t frame);
		void CreateGBufferPipeline(const spv_reflect::ShaderModule* vertexShaderModule, const spv_reflect::ShaderModule* fragmentShaderModule, const FrameGraphConfig* frameGraphConfig, VkPipeline* outPipeline);

	private:
		VkRenderPass  mRenderPass;
		VkFramebuffer mFramebuffer;

		VkPipelineLayout mPipelineLayout;
		VkPipeline       mStaticPipeline;
		VkPipeline       mRigidPipeline;

		VkDescriptorSet mSamplerSet;
		VkDescriptorSet mSceneMaterialDataSet;
		VkDescriptorSet mSceneObjectDataSet;

		uint32_t mSamplerSetNumber;
		uint32_t mSceneMaterialSetNumber;
		uint32_t mSceneObjectSetNumber;

		uint32_t           mMaterialIndexPushConstantOffset;
		VkShaderStageFlags mMaterialIndexPushConstantStages;

		uint32_t           mObjectIndexPushConstantOffset;
		VkShaderStageFlags mObjectIndexPushConstantStages;
	};
}