#pragma once

#include "VulkanRenderPass.hpp"
#include "VulkanFrameGraph.hpp"
#include <unordered_map>
#include <memory>
#include <array>
#include <variant>
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

	using RenderPassCreateFunc = std::unique_ptr<RenderPass>(*)(VkDevice, const FrameGraphBuilder*, const std::string&);

	class FrameGraphBuilder: ModernFrameGraphBuilder
	{
		constexpr static uint32_t ImageFlagAutoBarrier = 0x01;

		struct SubresourceInfo
		{
			VkFormat             Format;
			VkImageLayout        Layout;
			VkImageUsageFlags    Usage;
			VkImageAspectFlags   Aspect;
			VkPipelineStageFlags Stage;
			VkAccessFlags        Access;
			uint32_t             Flags;
		};

	public:
		FrameGraphBuilder(FrameGraph* graphToBuild, const DescriptorManager* descriptorManager, const InstanceParameters* instanceParameters, const DeviceParameters* deviceParameters, const ShaderManager* shaderManager, const MemoryManager* memoryManager, const DeviceQueues* deviceQueues, const SwapChain* swapchain);
		~FrameGraphBuilder();

		void RegisterRenderPass(const std::string_view passName, RenderPassCreateFunc createFunc, RenderPassType passType);

		void SetPassSubresourceFormat(const std::string_view passName,      const std::string_view subresourceId, VkFormat format);
		void SetPassSubresourceLayout(const std::string_view passName,      const std::string_view subresourceId, VkImageLayout layout);
		void SetPassSubresourceUsage(const std::string_view passName,       const std::string_view subresourceId, VkImageUsageFlags usage);
		void SetPassSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId, VkImageAspectFlags aspect);
		void SetPassSubresourceStageFlags(const std::string_view passName,  const std::string_view subresourceId, VkPipelineStageFlags stage);
		void SetPassSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId, VkAccessFlags accessFlags);

		void EnableSubresourceAutoBarrier(const std::string_view passName, const std::string_view subresourceId, bool autoBaarrier = true);

		const DeviceParameters*  GetDeviceParameters()  const;
		const MemoryManager*     GetMemoryManager()     const;
		const ShaderManager*     GetShaderManager()     const;
		const DescriptorManager* GetDescriptorManager() const;
		const DeviceQueues*      GetDeviceQueues()      const;
		const SwapChain*         GetSwapChain()         const;

		VkImage              GetRegisteredResource(const std::string_view passName,               const std::string_view subresourceId) const;
		VkImageView          GetRegisteredSubresource(const std::string_view passName,            const std::string_view subresourceId) const;
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
		//Functions for creating frame graph passes and resources
		void BuildPassObjects(const std::unordered_set<RenderPassName>& swapchainPassNames);

		//Builds barriers
		void BuildBarriers();

		//Transit images from UNDEFINED to usable state
		void BarrierImages(const DeviceQueues* deviceQueues, const WorkerCommandBuffers* workerCommandBuffers, uint32_t defaultQueueIndex);

		//Set object name for debug messages
		void SetDebugObjectName(VkImage image, const std::string_view name) const;

		//Converts pass type to a queue family index
		uint32_t PassTypeToQueueIndex(RenderPassType passType) const;

	private:
		//Creates a new subresource info record
		uint32_t AddSubresourceMetadata() override;

		//Finds all indices in subresourceInfoIndices that refer to non-unique entries in mSubresourceInfos
		//Returns a list of indices pointing to unique entries. Also builds a map to match the old list to the new list
		void BuildUniqueSubresourceList(const std::vector<uint32_t>& subresourceInfoIndices, std::vector<uint32_t>& outUniqueSubresourceInfoIndices, std::vector<uint32_t>& outNewIndexMap) override;

		//Propagates info (format, access flags, etc.) from one SubresourceInfo to another. Returns true if propagation succeeded or wasn't needed
		bool PropagateSubresourceParameters(uint32_t indexFrom, uint32_t indexTo) override;

		//Creates image objects
		void CreateTextures(const std::vector<TextureResourceCreateInfo>& textureCreateInfos) const override;

		//Creates image view objects
		void CreateTextureViews(const std::vector<TextureResourceCreateInfo>& textureCreateInfos, const std::vector<uint32_t>& subresourceInfoIndices) const override;

		//Initializes backbuffer-related 
		uint32_t AllocateBackbufferResources() const override;

	private:
		FrameGraph* mVulkanGraphToBuild;

		std::unordered_map<RenderPassName, RenderPassCreateFunc> mRenderPassCreateFunctions;

		std::vector<SubresourceInfo> mSubresourceInfos;

		//Several things that might be needed during build
		const DeviceQueues*       mDeviceQueues;
		const SwapChain*          mSwapChain;
		const DescriptorManager*  mDescriptorManager;
		const InstanceParameters* mInstanceParameters;
		const DeviceParameters*   mDeviceParameters;
		const ShaderManager*      mShaderManager;
		const MemoryManager*      mMemoryManager;
	};
}