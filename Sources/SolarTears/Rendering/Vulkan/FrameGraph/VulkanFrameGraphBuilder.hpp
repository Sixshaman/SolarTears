#pragma once

#include "VulkanRenderPass.hpp"
#include "VulkanFrameGraph.hpp"
#include "VulkanFrameGraphMisc.hpp"
#include <unordered_map>
#include <memory>
#include <array>
#include <span>
#include "../../../Core/DataStructures/Span.hpp"
#include "../../Common/FrameGraph/ModernFrameGraphBuilder.hpp"

namespace Vulkan
{
	class FrameGraph;
	class MemoryManager;
	class DeviceQueues;
	class InstanceParameters;
	class RenderableScene;
	class DeviceParameters;
	class SamplerManager;
	class ShaderDatabase;
	class DescriptorDatabase;
	class SharedDescriptorDatabaseBuilder;
	class PassDescriptorDatabaseBuilder;

	struct FrameGraphBuildInfo
	{
		const InstanceParameters*   InstanceParams;
		const DeviceParameters*     DeviceParams; 
		const MemoryManager*        MemoryAllocator; 
		const DeviceQueues*         Queues;
		const WorkerCommandBuffers* CommandBuffers;
	};

	class FrameGraphBuilder final: public ModernFrameGraphBuilder
	{
		friend class PassDescriptorDatabaseBuilder;

	public:
		FrameGraphBuilder(LoggerQueue* logger, FrameGraph* graphToBuild, SamplerManager* samplerManager, const SwapChain* swapchain);
		~FrameGraphBuilder();

		const VkDevice          GetDevice()           const;
		const DeviceParameters* GetDeviceParameters() const;
		const DeviceQueues*     GetDeviceQueues()     const;
		const SwapChain*        GetSwapChain()        const;
		
		ShaderDatabase* GetShaderDatabase() const;

		VkImage              GetRegisteredResource(uint32_t passIndex,    uint_fast16_t subresourceIndex) const;
		VkImageView          GetRegisteredSubresource(uint32_t passIndex, uint_fast16_t subresourceIndex) const;

		VkFormat             GetRegisteredSubresourceFormat(uint32_t passIndex,      uint_fast16_t subresourceIndex) const;
		VkImageLayout        GetRegisteredSubresourceLayout(uint32_t passIndex,      uint_fast16_t subresourceIndex) const;
		VkImageUsageFlags    GetRegisteredSubresourceUsage(uint32_t passIndex,       uint_fast16_t subresourceIndex) const;
		VkPipelineStageFlags GetRegisteredSubresourceStageFlags(uint32_t passIndex,  uint_fast16_t subresourceIndex) const;
		VkImageAspectFlags   GetRegisteredSubresourceAspectFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const;
		VkAccessFlags        GetRegisteredSubresourceAccessFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const;

		VkImageLayout        GetPreviousPassSubresourceLayout(uint32_t passIndex,     uint_fast16_t subresourceIndex) const;
		VkImageUsageFlags    GetPreviousPassSubresourceUsage(uint32_t passIndex,      uint_fast16_t subresourceIndex) const;
		VkPipelineStageFlags GetPreviousPassSubresourceStageFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const;
		VkImageAspectFlags   GetPreviousPassSubresourceAspectFlags(uint32_t passName, uint_fast16_t subresourceIndex) const;
		VkAccessFlags        GetPreviousPassSubresourceAccessFlags(uint32_t passName, uint_fast16_t subresourceIndex) const;

		VkImageLayout        GetNextPassSubresourceLayout(uint32_t passIndex,      uint_fast16_t subresourceIndex) const;
		VkImageUsageFlags    GetNextPassSubresourceUsage(uint32_t passIndex,       uint_fast16_t subresourceIndex) const;
		VkPipelineStageFlags GetNextPassSubresourceStageFlags(uint32_t passIndex,  uint_fast16_t subresourceIndex) const;
		VkImageAspectFlags   GetNextPassSubresourceAspectFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const;
		VkAccessFlags        GetNextPassSubresourceAccessFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const;

		void Build(FrameGraphDescription&& frameGraphDescription, const FrameGraphBuildInfo& buildInfo);
		void ValidateDescriptors(DescriptorDatabase* descriptorDatabase, SharedDescriptorDatabaseBuilder* sharedDatabaseBuilder, PassDescriptorDatabaseBuilder* passDatabaseBuilder);

	private:
		//Converts pass type to a queue family index
		uint32_t PassClassToQueueIndex(RenderPassClass passClass) const;

		//Creates an image view
		VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect) const;

	private:
		//Registers subresource api-specific metadata
		void InitMetadataPayloads() override final;

		//Propagates API-specific subresource data from one subresource to another, within a single resource
		bool PropagateSubresourcePayloadDataVertically(const ResourceMetadata& resourceMetadata) override final;

		//Propagates API-specific subresource data from one subresource to another, within a single pass
		bool PropagateSubresourcePayloadDataHorizontally(const PassMetadata& passMetadata) override final;

		//Creates image objects
		void CreateTextures() override final;

		//Creates image view objects
		void CreateTextureViews() override final;

		//Build the render pass objects
		void BuildPassObjects() override final;

		//Adds subresource barriers to execute before a pass
		void CreateBeforePassBarriers(const PassMetadata& passMetadata, uint32_t barrierSpanIndex) override final;

		//Adds subresource barriers to execute after a pass
		void CreateAfterPassBarriers(const PassMetadata& passMetadata, uint32_t barrierSpanIndex) override final;

		//Initializes per-traverse command buffer info
		void InitializeTraverseData() const override final;

		//Get the number of swapchain images
		uint32_t GetSwapchainImageCount() const override final;

	private:
		FrameGraph* mVulkanGraphToBuild;

		LoggerQueue* mLogger;

		std::vector<SubresourceMetadataPayload> mSubresourceMetadataPayloads;

		//Several things that might be needed during build
		const DeviceQueues*         mDeviceQueues;
		const SwapChain*            mSwapChain;
		const WorkerCommandBuffers* mWorkerCommandBuffers;
		const InstanceParameters*   mInstanceParameters;
		const DeviceParameters*     mDeviceParameters;
		const MemoryManager*        mMemoryManager;

		std::unique_ptr<ShaderDatabase> mShaderDatabase;
	};
}