#pragma once

#include "../VulkanRenderPass.hpp"
#include "../../../Common/FrameGraph/Passes/GBufferPass.hpp"
#include "../../../3rdParty/SPIRV-Reflect/spirv_reflect.h"
#include <span>

namespace Vulkan
{
	class FrameGraphBuilder;
	class DeviceParameters;
	class DescriptorManager;
	class RenderableScene;

	class GBufferPass: public RenderPass, public GBufferPassBase
	{
		static constexpr uint32_t StaticVertexShaderIndex          = 0;
		static constexpr uint32_t StaticInstancedVertexShaderIndex = 1;
		static constexpr uint32_t RigidVertexShaderIndex           = 2;
		static constexpr uint32_t FragmentShaderIndex              = 3;

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

		std::span<VkDescriptorSet> mSceneCommonAndStaticDescriptorSets; //All descriptor sets for static object drawing (materials, textures, frame + object data)
		std::span<VkDescriptorSet> mSceneRigidDescriptorSets;           //Just the object data descriptor sets for dynamic objects (frame + dynamic object data)
		uint32_t                   mSceneObjectDataSetNumber;           //The descriptor set for object data

		uint32_t           mMaterialIndexPushConstantOffset;
		VkShaderStageFlags mMaterialIndexPushConstantStages;

		uint32_t           mObjectIndexPushConstantOffset;
		VkShaderStageFlags mObjectIndexPushConstantStages;
	};
}