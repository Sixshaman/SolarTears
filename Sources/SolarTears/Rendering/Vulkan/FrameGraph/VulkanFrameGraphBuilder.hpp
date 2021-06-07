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
		};

	public:
		FrameGraphBuilder(FrameGraph* graphToBuild, const DescriptorManager* descriptorManager, const InstanceParameters* instanceParameters, const DeviceParameters* deviceParameters, const ShaderManager* shaderManager);
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
		const ShaderManager*     GetShaderManager()     const;
		const DescriptorManager* GetDescriptorManager() const;
		const FrameGraphConfig*  GetConfig()            const;

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

		//Functions for creating actual frame graph resources/subresources
		void SetSwapchainImages(const std::unordered_map<SubresourceName, BackbufferResourceCreateInfo>& backbufferResourceCreateInfos, const std::vector<VkImage>& swapchainImages);

		//Creates VkImageView from imageViewInfo
		void CreateImageView(const ImageViewInfo& imageViewInfo, VkImage image, VkFormat defaultFormat, VkImageView* outImageView);

		void PlaceholderFunc(const std::vector<TextureResourceCreateInfo>& textureCreateInfos);

		//Set object name for debug messages
		void SetDebugObjectName(VkImage image, const SubresourceName& name) const;

	private:
		//Propagates info (format, access flags, etc.) from one SubresourceInfo to another. Returns true if propagation succeeded or wasn't needed
		virtual bool PropagateSubresourceParameters(uint32_t indexFrom, uint32_t indexTo);

		//Creates image objects
		virtual void CreateImages(const std::vector<TextureResourceCreateInfo>& textureCreateInfos) const override;

	private:
		FrameGraph* mGraphToBuild;

		std::unordered_map<RenderPassName, RenderPassCreateFunc> mRenderPassCreateFunctions;

		std::vector<SubresourceInfo> mSubresourceInfos;

		//Several things that might be needed to create some of the passes
		const DescriptorManager*  mDescriptorManager;
		const InstanceParameters* mInstanceParameters;
		const DeviceParameters*   mDeviceParameters;
		const ShaderManager*      mShaderManager;
	};
}