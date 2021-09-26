#pragma once

#include "VulkanRenderPass.hpp"
#include "VulkanFrameGraph.hpp"
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
	class ShaderDatabase;

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
	public:
		FrameGraphBuilder(LoggerQueue* logger, FrameGraph* graphToBuild, FrameGraphDescription&& frameGraphDescription, const SwapChain* swapchain);
		~FrameGraphBuilder();

		void SetPassSubresourceFormat(const std::string_view subresourceId, VkFormat format);
		void SetPassSubresourceLayout(const std::string_view subresourceId, VkImageLayout layout);
		void SetPassSubresourceUsage(const std::string_view subresourceId, VkImageUsageFlags usage);
		void SetPassSubresourceAspectFlags(const std::string_view subresourceId, VkImageAspectFlags aspect);
		void SetPassSubresourceStageFlags(const std::string_view subresourceId, VkPipelineStageFlags stage);
		void SetPassSubresourceAccessFlags(const std::string_view subresourceId, VkAccessFlags accessFlags);

		void EnableSubresourceAutoBeforeBarrier(const std::string_view subresourceId, bool autoBarrier = true);
		void EnableSubresourceAutoAfterBarrier(const std::string_view subresourceId,  bool autoBarrier = true);

		const VkDevice          GetDevice()           const;
		const DeviceParameters* GetDeviceParameters() const;
		const DeviceQueues*     GetDeviceQueues()     const;
		const SwapChain*        GetSwapChain()        const;
		
		ShaderDatabase* GetShaderDatabase() const;

		VkImage              GetRegisteredResource(const std::string_view passName,    const std::string_view subresourceId, uint32_t frame) const;
		VkImageView          GetRegisteredSubresource(const std::string_view passName, const std::string_view subresourceId, uint32_t frame) const;

		VkFormat             GetRegisteredSubresourceFormat(const std::string_view subresourceId) const;
		VkImageLayout        GetRegisteredSubresourceLayout(const std::string_view subresourceId) const;
		VkImageUsageFlags    GetRegisteredSubresourceUsage(const std::string_view subresourceId) const;
		VkPipelineStageFlags GetRegisteredSubresourceStageFlags(const std::string_view subresourceId) const;
		VkImageAspectFlags   GetRegisteredSubresourceAspectFlags(const std::string_view subresourceId) const;
		VkAccessFlags        GetRegisteredSubresourceAccessFlags(const std::string_view subresourceId) const;

		VkImageLayout        GetPreviousPassSubresourceLayout(const std::string_view subresourceId) const;
		VkImageUsageFlags    GetPreviousPassSubresourceUsage(const std::string_view subresourceId) const;
		VkPipelineStageFlags GetPreviousPassSubresourceStageFlags(const std::string_view subresourceId) const;
		VkImageAspectFlags   GetPreviousPassSubresourceAspectFlags(const std::string_view subresourceId) const;
		VkAccessFlags        GetPreviousPassSubresourceAccessFlags(const std::string_view subresourceId) const;

		VkImageLayout        GetNextPassSubresourceLayout(const std::string_view subresourceId) const;
		VkImageUsageFlags    GetNextPassSubresourceUsage(const std::string_view subresourceId) const;
		VkPipelineStageFlags GetNextPassSubresourceStageFlags(const std::string_view subresourceId) const;
		VkImageAspectFlags   GetNextPassSubresourceAspectFlags(const std::string_view subresourceId) const;
		VkAccessFlags        GetNextPassSubresourceAccessFlags(const std::string_view subresourceId) const;

		void Build(const FrameGraphBuildInfo& buildInfo);

		void BuildPassDescriptorSets();

	private:
		//Converts pass type to a queue family index
		uint32_t PassClassToQueueIndex(RenderPassClass passClass) const;

		//Creates an image view
		VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect) const;

	private:
		//Registers subresource ids for pass types
		void RegisterPassTypes(const std::span<RenderPassType>& passTypes) override final;

		//Checks if the usage of the subresource with subresourceInfoIndex includes reading
		bool IsReadSubresource(uint32_t subresourceInfoIndex) override final;

		//Checks if the usage of the subresource with subresourceInfoIndex includes writing
		bool IsWriteSubresource(uint32_t subresourceInfoIndex) override final;

		//Creates a new render pass
		void CreatePassObject(const FrameGraphDescription::RenderPassName& passName, RenderPassType passType, uint32_t frame) override final;

		//Gives a free render pass span id
		uint32_t NextPassSpanId() override final;

		//Creates image objects
		void CreateTextures() override final;

		//Creates image view objects
		void CreateTextureViews() override final;

		//Adds a barrier to execute before a pass
		uint32_t AddBeforePassBarrier(uint32_t metadataIndex) override final;

		//Adds a barrier to execute before a pass
		uint32_t AddAfterPassBarrier(uint32_t metadataIndex) override final;

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

#include "VulkanFrameGraphBuilder.inl"