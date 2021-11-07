#pragma once

#include "../VulkanRenderPass.hpp"
#include "../VulkanFrameGraphMisc.hpp"
#include "../../VulkanShaders.hpp"
#include "../../../Common/FrameGraph/Passes/GBufferPass.hpp"
#include "../../../Common/RenderingUtils.hpp"
#include "../../../../Core/DataStructures/Span.hpp"
#include "../../../../Core/Util.hpp"
#include <span>
#include <string>
#include <array>

namespace Vulkan
{
	class DeviceParameters;

	class GBufferPass: public RenderPass, public GBufferPassBase
	{
		constexpr static std::string_view StaticShaderGroup = "GBufferStaticShaders";
		constexpr static std::string_view RigidShaderGroup  = "GBufferRigidShaders";

	public:
		inline static void             RegisterSubresources(std::span<SubresourceMetadataPayload> inoutMetadataPayloads);
		inline static void             RegisterShaders(ShaderDatabase* shaderDatabase);
		inline static bool             PropagateSubresourceInfos(std::span<SubresourceMetadataPayload> inoutMetadataPayloads);
		inline static VkDescriptorType GetSubresourceDescriptorType(uint_fast16_t subresourceId);

	public:
		GBufferPass(const FrameGraphBuilder* frameGraphBuilder, uint32_t frameGraphPassIndex);
		~GBufferPass();

		void RecordExecution(VkCommandBuffer commandBuffer, const RenderableScene* renderableScene, const FrameGraphConfig& frameGraphConfig) const override;

		void ValidateDescriptorSetSpans(DescriptorDatabase* descriptorDatabase, const VkDescriptorSet* originalSetStartPoint) override;

	private:
		void ObtainDescriptorData(ShaderDatabase* shaderDatabase, uint32_t passIndex);
		void CreateRenderPass(const FrameGraphBuilder* frameGraphBuilder, uint32_t frameGraphPassId, const DeviceParameters* deviceParameters);
		void CreateFramebuffer(const FrameGraphBuilder* frameGraphBuilder, uint32_t frameGraphPassId, const FrameGraphConfig* frameGraphConfig);
		void CreatePipelineLayouts(const ShaderDatabase* shaderDatabase);
		void CreatePipelines(const ShaderDatabase* shaderDatabase, const FrameGraphConfig* frameGraphConfig);

		void CreateGBufferPipeline(const uint32_t* vertexShaderModule, uint32_t vertexShaderModuleSize, const uint32_t* fragmentShaderModule, uint32_t fragmetShaderModuleSize, VkPipelineLayout pipelineLayout, const FrameGraphConfig* frameGraphConfig, VkPipeline* outPipeline);

	private:
		VkRenderPass  mRenderPass;
		VkFramebuffer mFramebuffer;

		VkPipelineLayout mStaticPipelineLayout;
		VkPipelineLayout mRigidPipelineLayout;

		VkPipeline mStaticPipeline;
		VkPipeline mRigidPipeline;

		std::span<VkDescriptorSet> mDescriptorSets;           //All descriptor sets needed for the pass
		Span<uint32_t>             mStaticDrawChangedSetSpan; //The span in mDescriptorSets to bind for static meshes draw
		Span<uint32_t>             mRigidDrawChangedSetSpan;  //The span in mDescriptorSets to bind for rigid meshes draw
		uint32_t                   mStaticDrawSetBindOffset;  //The firstSet parameter for static meshes draw 
		uint32_t                   mRigidDrawSetBindOffset;   //The firstSet parameter for rigid meshes draw 

		uint32_t           mMaterialIndexPushConstantOffset;
		VkShaderStageFlags mMaterialIndexPushConstantStages;

		uint32_t           mObjectIndexPushConstantOffset;
		VkShaderStageFlags mObjectIndexPushConstantStages;
	};
}

#include "VulkanGBufferPass.inl"