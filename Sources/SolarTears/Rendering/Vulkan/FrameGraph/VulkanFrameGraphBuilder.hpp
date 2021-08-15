#pragma once

#include "VulkanRenderPass.hpp"
#include "VulkanFrameGraph.hpp"
#include <unordered_map>
#include <memory>
#include <array>
#include "../../../Core/DataStructures/Span.hpp"
#include "../../Common/FrameGraph/ModernFrameGraphBuilder.hpp"
#include "../../Vulkan/Scene/VulkanSceneDescriptorDatabase.hpp"

namespace Vulkan
{
	class FrameGraph;
	class MemoryManager;
	class DeviceQueues;
	class InstanceParameters;
	class RenderableScene;
	class DeviceParameters;
	class ShaderDatabase;
	class SceneDescriptorDatabase;
	class PassDescriptorDatabase;
	class SamplerDescriptorDatabase;

	using RenderPassCreateFunc = std::unique_ptr<RenderPass>(*)(VkDevice, const FrameGraphBuilder*, const std::string&, uint32_t);
	using RenderPassAddFunc    = void(*)(FrameGraphBuilder* frameGraphBuilder, const std::string& passName);

	struct FrameGraphBuildInfo
	{
		const InstanceParameters*   InstanceParams;
		const DeviceParameters*     DeviceParams; 
		const MemoryManager*        MemoryAllocator; 
		const DeviceQueues*         Queues;
		const WorkerCommandBuffers* CommandBuffers;

		SceneDescriptorDatabase*   DescriptorDatabaseScene;
		PassDescriptorDatabase*    DescriptorDatabasePasses;
		SamplerDescriptorDatabase* DescriptorDatabaseSamplers;
	};

	class FrameGraphBuilder final: public ModernFrameGraphBuilder
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
		FrameGraphBuilder(LoggerQueue* logger, FrameGraph* graphToBuild, FrameGraphDescription&& frameGraphDescription, const SwapChain* swapchain);
		~FrameGraphBuilder();

		void SetPassSubresourceFormat(const std::string_view passName,      const std::string_view subresourceId, VkFormat format);
		void SetPassSubresourceLayout(const std::string_view passName,      const std::string_view subresourceId, VkImageLayout layout);
		void SetPassSubresourceUsage(const std::string_view passName,       const std::string_view subresourceId, VkImageUsageFlags usage);
		void SetPassSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId, VkImageAspectFlags aspect);
		void SetPassSubresourceStageFlags(const std::string_view passName,  const std::string_view subresourceId, VkPipelineStageFlags stage);
		void SetPassSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId, VkAccessFlags accessFlags);

		const DeviceParameters*  GetDeviceParameters()  const;
		const DeviceQueues*      GetDeviceQueues()      const;
		const SwapChain*         GetSwapChain()         const;

		ShaderDatabase*            GetShaderDatabase()            const;
		SceneDescriptorDatabase*   GetSceneDescriptorDatabase()   const;
		PassDescriptorDatabase*    GetPassDescriptorDatabase()    const;
		SamplerDescriptorDatabase* GetSamplerDescriptorDatabase() const;

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

		void Build(const FrameGraphBuildInfo& buildInfo);

	private:
		//Adds a render pass of type Pass to the frame graph pass table
		template<typename Pass>
		void AddPassToTable();
		
		//Initializes frame graph pass table
		void InitPassTable();

		//Converts pass type to a queue family index
		uint32_t PassClassToQueueIndex(RenderPassClass passClass) const;

		//Creates an image view
		VkImageView CreateImageView(VkImage image, uint32_t subresourceInfoIndex) const;

		//Creates a descriptor set layout
		VkDescriptorSetLayout CreateDescriptorSetLayout(std::span<VkDescriptorSetLayoutBinding> bindings, std::span<VkDescriptorBindingFlags> bindingFlags);

	private:
		//Creates a new subresource info record
		uint32_t AddSubresourceMetadata() override final;

		//Creates a new subresource info record for present pass
		uint32_t AddPresentSubresourceMetadata() override final;

		//Registers render pass related data in the graph (inputs, outputs, possibly shader bindings, etc)
		void RegisterPassInGraph(RenderPassType passType, const FrameGraphDescription::RenderPassName& passName) override final;

		//Creates a new render pass
		void CreatePassObject(const FrameGraphDescription::RenderPassName& passName, RenderPassType passType, uint32_t frame) override final;

		//Gives a free render pass span id
		uint32_t NextPassSpanId() override final;

		//Propagates subresource info (format, access flags, etc.) to a node from the previous one. Also initializes view key. Returns true if propagation succeeded or wasn't needed
		bool ValidateSubresourceViewParameters(SubresourceMetadataNode* currNode, SubresourceMetadataNode* prevNode) override final;

		//Allocates the storage for image views defined by sort keys
		void AllocateImageViews(const std::vector<uint64_t>& sortKeys, uint32_t frameCount, std::vector<uint32_t>& outViewIds) override final;

		//Creates image objects
		void CreateTextures(const std::vector<TextureResourceCreateInfo>& textureCreateInfos, const std::vector<TextureResourceCreateInfo>& backbufferCreateInfos, uint32_t totalTextureCount) const override final;

		//Creates image view objects
		void CreateTextureViews(const std::vector<TextureSubresourceCreateInfo>& textureViewCreateInfos) const override final;

		//Adds a barrier to execute before a pass
		uint32_t AddBeforePassBarrier(uint32_t imageIndex, RenderPassClass prevPassClass, uint32_t prevPassSubresourceInfoIndex, RenderPassClass currPassClass, uint32_t currPassSubresourceInfoIndex) override final;

		//Adds a barrier to execute before a pass
		uint32_t AddAfterPassBarrier(uint32_t imageIndex, RenderPassClass currPassClass, uint32_t currPassSubresourceInfoIndex, RenderPassClass nextPassClass, uint32_t nextPassSubresourceInfoIndex) override final;

		//Initializes per-traverse command buffer info
		void InitializeTraverseData() const override final;

		//Get the number of swapchain images
		uint32_t GetSwapchainImageCount() const override final;

	private:
		FrameGraph* mVulkanGraphToBuild;

		LoggerQueue* mLogger;

		std::unordered_map<RenderPassType, RenderPassAddFunc>    mPassAddFuncTable;
		std::unordered_map<RenderPassType, RenderPassCreateFunc> mPassCreateFuncTable;

		std::vector<SubresourceInfo> mSubresourceInfos;

		uint32_t mImageViewCount;

		//Several things that might be needed during build
		const DeviceQueues*         mDeviceQueues;
		const SwapChain*            mSwapChain;
		const WorkerCommandBuffers* mWorkerCommandBuffers;
		const InstanceParameters*   mInstanceParameters;
		const DeviceParameters*     mDeviceParameters;
		const MemoryManager*        mMemoryManager;

		std::unique_ptr<ShaderDatabase> mShaderDatabase;
		SceneDescriptorDatabase*        mSceneDescriptorDatabase;
		PassDescriptorDatabase*         mPassDescriptorDatabase;
		SamplerDescriptorDatabase*      mSamplerDescriptorDatabase;
	};
}

#include "VulkanFrameGraphBuilder.inl"