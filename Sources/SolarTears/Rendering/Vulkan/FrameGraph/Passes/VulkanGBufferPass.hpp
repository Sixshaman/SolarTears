#pragma once

#include "../VulkanRenderPass.hpp"
#include "../../../Common/FrameGraph/Passes/GBufferPass.hpp"
#include "../../../../Core/DataStructures/Span.hpp"
#include "../VulkanFrameGraphMisc.hpp"
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
		constexpr static std::string_view StaticShaderGroup          = "GBufferStaticShaders";
		constexpr static std::string_view StaticInstancedShaderGroup = "GBufferStaticInstancedShaders";
		constexpr static std::string_view RigidShaderGroup           = "GBufferRigidShaders";

	public:
		constexpr inline static VkFormat             GetSubresourceFormat(PassSubresourceId subresourceId);
		constexpr inline static VkImageAspectFlags   GetSubresourceAspect(PassSubresourceId subresourceId);
		constexpr inline static VkPipelineStageFlags GetSubresourceStage(PassSubresourceId subresourceId);
		constexpr inline static VkImageLayout        GetSubresourceLayout(PassSubresourceId subresourceId);
		constexpr inline static VkImageUsageFlags    GetSubresourceUsage(PassSubresourceId subresourceId);
		constexpr inline static VkAccessFlags        GetSubresourceAccess(PassSubresourceId subresourceId);

		inline static void RegisterShaders(ShaderDatabase* shaderDatabase);

	public:
		GBufferPass(const FrameGraphBuilder* frameGraphBuilder, const std::string& passName, uint32_t frame);
		~GBufferPass();

		void RecordExecution(VkCommandBuffer commandBuffer, const RenderableScene* renderableScene, const FrameGraphConfig& frameGraphConfig) const override;

	private:
		void CreateRenderPass(const FrameGraphBuilder* frameGraphBuilder, const DeviceParameters* deviceParameters, const std::string& currentPassName);
		void CreateFramebuffer(const FrameGraphBuilder* frameGraphBuilder, const FrameGraphConfig* frameGraphConfig, const std::string& currentPassName, uint32_t frame);
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

		std::span<VkDescriptorSet> mDescriptorSets;
		Span<uint32_t>             mStaticDrawChangedSetSpan;
		Span<uint32_t>             mRigidDrawChangedSetSpan;
		uint32_t                   mStaticDrawSetBindOffset;
		uint32_t                   mRigidDrawSetBindOffset;

		uint32_t           mMaterialIndexPushConstantOffset;
		VkShaderStageFlags mMaterialIndexPushConstantStages;

		uint32_t           mObjectIndexPushConstantOffset;
		VkShaderStageFlags mObjectIndexPushConstantStages;
	};
}

#include "VulkanGBufferPass.inl"