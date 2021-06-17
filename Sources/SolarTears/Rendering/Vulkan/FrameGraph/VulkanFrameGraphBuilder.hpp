#pragma once

#include "VulkanRenderPass.hpp"
#include "VulkanFrameGraph.hpp"
#include <unordered_map>
#include <memory>
#include <array>
#include "../../Common/FrameGraph/ModernFrameGraphBuilder.hpp"

namespace Vulkan
{
	class FrameGraph;
	class MemoryManager;
	class DeviceQueues;
	class InstanceParameters;
	class RenderableScene;
	class DeviceParameters;
	class ShaderManager;
	class DescriptorManager;

	using RenderPassCreateFunc = std::unique_ptr<RenderPass>(*)(VkDevice, const FrameGraphBuilder*, const std::string&, uint32_t);

	class FrameGraphBuilder: public ModernFrameGraphBuilder
	{
		struct SubresourceInfo
		{
			VkFormat             Format;
			VkImageLayout        Layout;
			VkImageUsageFlags    Usage;
			VkImageAspectFlags   Aspect;
			VkPipelineStageFlags Stage;
			VkAccessFlags        Access;
		};

	public:
		FrameGraphBuilder(FrameGraph* graphToBuild, const DescriptorManager* descriptorManager, const InstanceParameters* instanceParameters, const DeviceParameters* deviceParameters, const ShaderManager* shaderManager, const MemoryManager* memoryManager, const DeviceQueues* deviceQueues, const SwapChain* swapchain, const WorkerCommandBuffers* workerCommandBuffers);
		~FrameGraphBuilder();

		void RegisterRenderPass(const std::string_view passName, RenderPassCreateFunc createFunc, RenderPassType passType);

		void SetPassSubresourceFormat(const std::string_view passName,      const std::string_view subresourceId, VkFormat format);
		void SetPassSubresourceLayout(const std::string_view passName,      const std::string_view subresourceId, VkImageLayout layout);
		void SetPassSubresourceUsage(const std::string_view passName,       const std::string_view subresourceId, VkImageUsageFlags usage);
		void SetPassSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId, VkImageAspectFlags aspect);
		void SetPassSubresourceStageFlags(const std::string_view passName,  const std::string_view subresourceId, VkPipelineStageFlags stage);
		void SetPassSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId, VkAccessFlags accessFlags);

		const DeviceParameters*  GetDeviceParameters()  const;
		const MemoryManager*     GetMemoryManager()     const;
		const ShaderManager*     GetShaderManager()     const;
		const DescriptorManager* GetDescriptorManager() const;
		const DeviceQueues*      GetDeviceQueues()      const;
		const SwapChain*         GetSwapChain()         const;

		VkImage              GetRegisteredResource(const std::string_view passName,    const std::string_view subresourceId, uint32_t frame) const;
		VkImageView          GetRegisteredSubresource(const std::string_view passName, const std::string_view subresourceId, uint32_t frame) const;

		VkFormat             GetRegisteredSubresourceFormat(const std::string_view passName,      const std::string_view subresourceId) const;
		VkImageLayout        GetRegisteredSubresourceLayout(const std::string_view passName,      const std::string_view subresourceId) const;
		VkImageUsageFlags    GetRegisteredSubresourceUsage(const std::string_view passName,       const std::string_view subresourceId) const;
		VkPipelineStageFlags GetRegisteredSubresourceStageFlags(const std::string_view passName,  const std::string_view subresourceId) const;
		VkImageAspectFlags   GetRegisteredSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId) const;
		VkAccessFlags        GetRegisteredSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId) const;

		VkImageLayout        GetPreviousPassSubresourceLayout(const std::string_view passName,      const std::string_view subresourceId) const;
		VkImageUsageFlags    GetPreviousPassSubresourceUsage(const std::string_view passName,       const std::string_view subresourceId) const;
		VkPipelineStageFlags GetPreviousPassSubresourceStageFlags(const std::string_view passName,  const std::string_view subresourceId) const;
		VkImageAspectFlags   GetPreviousPassSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId) const;
		VkAccessFlags        GetPreviousPassSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId) const;

		VkImageLayout        GetNextPassSubresourceLayout(const std::string_view passName,      const std::string_view subresourceId) const;
		VkImageUsageFlags    GetNextPassSubresourceUsage(const std::string_view passName,       const std::string_view subresourceId) const;
		VkPipelineStageFlags GetNextPassSubresourceStageFlags(const std::string_view passName,  const std::string_view subresourceId) const;
		VkImageAspectFlags   GetNextPassSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId) const;
		VkAccessFlags        GetNextPassSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId) const;

	private:
		//Converts pass type to a queue family index
		uint32_t PassTypeToQueueIndex(RenderPassType passType) const;

		//Creates an image view
		VkImageView CreateImageView(VkImage image, uint32_t subresourceInfoIndex) const;

	private:
		//Creates a new subresource info record
		uint32_t AddSubresourceMetadata() override;

		//Creates a new render pass
		void AddRenderPass(const RenderPassName& passName, uint32_t frame) override;

		//Gives a free render pass span id
		uint32_t NextPassSpanId() override;

		//Propagates subresource info (format, access flags, etc.) to a node from the previous one. Also initializes view key. Returns true if propagation succeeded or wasn't needed
		bool ValidateSubresourceViewParameters(SubresourceMetadataNode* node) override;

		//Allocates the storage for image views defined by sort keys
		void AllocateImageViews(const std::vector<uint64_t>& sortKeys, uint32_t frameCount, std::vector<uint32_t>& outViewIds) override;

		//Creates image objects
		void CreateTextures(const std::vector<TextureResourceCreateInfo>& textureCreateInfos, const std::vector<TextureResourceCreateInfo>& backbufferCreateInfos, uint32_t totalTextureCount) const override;

		//Creates image view objects
		void CreateTextureViews(const std::vector<TextureSubresourceCreateInfo>& textureViewCreateInfos) const override;

		//Initializes per-traverse command buffer info
		void InitializeTraverseData() const override;

		//Get the number of swapchain images
		uint32_t GetSwapchainImageCount() const override;

	private:
		FrameGraph* mVulkanGraphToBuild;

		std::unordered_map<RenderPassName, RenderPassCreateFunc> mRenderPassCreateFunctions;

		std::vector<SubresourceInfo> mSubresourceInfos;

		uint32_t mImageViewCount;

		//Several things that might be needed during build
		const DeviceQueues*         mDeviceQueues;
		const SwapChain*            mSwapChain;
		const WorkerCommandBuffers* mWorkerCommandBuffers;
		const DescriptorManager*    mDescriptorManager;
		const InstanceParameters*   mInstanceParameters;
		const DeviceParameters*     mDeviceParameters;
		const ShaderManager*        mShaderManager;
		const MemoryManager*        mMemoryManager;
	};
}