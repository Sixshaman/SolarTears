#pragma once

#include "../VulkanCRenderPass.hpp"

namespace VulkanCBindings
{
	class FrameGraphBuilder;

	class GBufferPass: public RenderPass
	{
	public:
		static constexpr std::string_view ColorBufferImageId = "GBufferPass-ColorBufferImage";

	public:
		~GBufferPass();

		void RecordExecution(VkCommandBuffer commandBuffer, const RenderableScene* scene, const FrameGraphConfig& frameGraphConfig) override;

		static void Register(FrameGraphBuilder* frameGraphBuilder, const std::string& passName);

	private:
		GBufferPass(VkDevice device, const FrameGraphBuilder* frameGraphBuilder, const std::string& passName);

	private:
		void CreateRenderPass(const FrameGraphBuilder* frameGraphBuilder, const DeviceParameters* deviceParameters, const std::string& currentPassName);
		void CreateFramebuffer(const FrameGraphBuilder* frameGraphBuilder, const FrameGraphConfig* frameGraphConfig, const std::string& currentPassName);
		void CreatePipelineLayout(const RenderableScene* sceneBaked1stPart);
		void CreatePipeline(const ShaderManager* shaderManager, const FrameGraphConfig* frameGraphConfig);

	private:
		VkRenderPass  mRenderPass;
		VkFramebuffer mFramebuffer;

		VkPipelineLayout mPipelineLayout;
		VkPipeline       mPipeline;
	};
}