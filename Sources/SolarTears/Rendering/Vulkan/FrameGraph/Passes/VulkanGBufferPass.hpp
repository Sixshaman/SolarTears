#pragma once

#include "../VulkanRenderPass.hpp"

namespace Vulkan
{
	class FrameGraphBuilder;
	class ShaderManager;
	class DeviceParameters;
	class DescriptorManager;
	class RenderableScene;

	class GBufferPass: public RenderPass
	{
	public:
		static constexpr std::string_view ColorBufferImageId = "GBufferPass-ColorBufferImage";

	public:
		~GBufferPass();

		void RecordExecution(VkCommandBuffer commandBuffer, const RenderableScene* renderableScene, const FrameGraphConfig& frameGraphConfig) const override;

		static void Register(FrameGraphBuilder* frameGraphBuilder, const std::string& passName);

	private:
		GBufferPass(VkDevice device, const FrameGraphBuilder* frameGraphBuilder, const std::string& passName);

	private:
		void CreateRenderPass(const FrameGraphBuilder* frameGraphBuilder, const DeviceParameters* deviceParameters, const std::string& currentPassName);
		void CreateFramebuffer(const FrameGraphBuilder* frameGraphBuilder, const FrameGraphConfig* frameGraphConfig, const std::string& currentPassName);
		void CreatePipelineLayout(const DescriptorManager* descriptorManager);
		void CreatePipeline(const ShaderManager* shaderManager, const FrameGraphConfig* frameGraphConfig);

	private:
		VkRenderPass  mRenderPass;
		VkFramebuffer mFramebuffer;

		VkPipelineLayout mPipelineLayout;
		VkPipeline       mPipeline;
	};
}