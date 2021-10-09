#pragma once

#include "../VulkanRenderPass.hpp"
#include "../../../Common/FrameGraph/Passes/GBufferPass.hpp"
#include "../../../../Core/DataStructures/Span.hpp"
#include "../VulkanFrameGraphMisc.hpp"
#include "../../../../Core/Util.hpp"
#include "../../../Common/RenderingUtils.hpp"
#include <span>

namespace Vulkan
{
	class FrameGraphBuilder;
	class DeviceParameters;
	class DescriptorManager;
	class RenderableScene;
	class ShaderDatabase;

	class GBufferPass: public RenderPass, public GBufferPassBase
	{
		constexpr static std::string_view StaticShaderGroup          = "GBufferStaticShaders";
		constexpr static std::string_view StaticInstancedShaderGroup = "GBufferStaticInstancedShaders";
		constexpr static std::string_view RigidShaderGroup           = "GBufferRigidShaders";

	public:
		inline static void             RegisterSubresources(std::span<SubresourceMetadataPayload> inoutMetadataPayloads);
		inline static void             RegisterShaders(ShaderDatabase* shaderDatabase);
		inline static bool             PropagateSubresourceInfos(std::span<SubresourceMetadataPayload> inoutMetadataPayloads);
		inline static VkDescriptorType GetSubresourceDescriptorType(uint_fast16_t subresourceId);

	public:
		GBufferPass(const FrameGraphBuilder* frameGraphBuilder, uint32_t frameGraphPassIndex);
		~GBufferPass();

		void RecordExecution(VkCommandBuffer commandBuffer, const RenderableScene* renderableScene, const FrameGraphConfig& frameGraphConfig, uint32_t frameResourceIndex) const override;

		void ValidateDescriptorSets(const ShaderDatabase* shaderDatabase, DescriptorDatabase* descriptorDatabase) override;

	private:
		void CreateRenderPass(const FrameGraphBuilder* frameGraphBuilder, uint32_t frameGraphPassId, const DeviceParameters* deviceParameters);
		void CreateFramebuffer(const FrameGraphBuilder* frameGraphBuilder, uint32_t frameGraphPassId, const FrameGraphConfig* frameGraphConfig);
		void CreatePipelineLayout(std::span<VkDescriptorSetLayout> setLayouts, std::span<VkPushConstantRange> pushConstantRanges, VkPipelineLayout* outPipelineLayout);
		void CreateGBufferPipeline(const uint32_t* vertexShaderModule, uint32_t vertexShaderModuleSize, const uint32_t* fragmentShaderModule, uint32_t fragmetShaderModuleSize, VkPipelineLayout pipelineLayout, const FrameGraphConfig* frameGraphConfig, VkPipeline* outPipeline);

	private:
		VkRenderPass  mRenderPass;
		VkFramebuffer mFramebuffer;

		VkPipelineLayout mStaticPipelineLayout; //Should match static instanced
		VkPipelineLayout mRigidPipelineLayout;

		VkPipeline mStaticPipeline;
		VkPipeline mStaticInstancedPipeline;
		VkPipeline mRigidPipeline;

		std::span<VkDescriptorSet> mDescriptorSets[Utils::InFlightFrameCount]; //All descriptor sets needed for pass (per frame resource)
		Span<uint32_t>             mStaticDrawChangedSetSpan;                  //The span in mDescriptorSets to bind for static mesh draw
		Span<uint32_t>             mRigidDrawChangedSetSpan;                   //The span in mDescriptorSets to bind for rigid mesh draw
		uint32_t                   mStaticDrawSetBindOffset;                   //The firstSet parameter for static mesh draw 
		uint32_t                   mRigidDrawSetBindOffset;                    //The firstSet parameter for rigid mesh draw 

		uint32_t           mMaterialIndexPushConstantOffset;
		VkShaderStageFlags mMaterialIndexPushConstantStages;

		uint32_t           mObjectIndexPushConstantOffset;
		VkShaderStageFlags mObjectIndexPushConstantStages;
	};
}

#include "VulkanGBufferPass.inl"