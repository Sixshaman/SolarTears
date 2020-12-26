#pragma once

#include "VulkanCRenderPass.hpp"
#include "VulkanCFrameGraph.hpp"
#include <unordered_map>
#include <memory>

namespace VulkanCBindings
{
	using RenderPassCreateFunc = std::unique_ptr<RenderPass>(*)(VkDevice, const FrameGraphBuilder*, const std::string&);

	class FrameGraphBuilder
	{
		using RenderPassName  = std::string;
		using SubresourceId   = std::string;
		using SubresourceName = std::string;

		struct ImageSubresourceMetadata
		{
			uint32_t             ImageIndex;
			uint32_t             ImageViewIndex;
			VkImageLayout        Layout;
			VkImageUsageFlags    UsageFlags;
			VkImageAspectFlags   AspectFlags;
			VkPipelineStageFlags PipelineStageFlags;
			VkAccessFlags        AccessFlags;
		};

	public:
		FrameGraphBuilder(FrameGraph* graphToBuild, const RenderableScene* scene, const DeviceParameters* deviceParameters, const ShaderManager* shaderManager);
		~FrameGraphBuilder();

		void RegisterRenderPass(const std::string_view passName, RenderPassCreateFunc createFunc);
		void RegisterReadSubresource(const std::string_view passName, const std::string_view subresourceId);
		void RegisterWriteSubresource(const std::string_view passName, const std::string_view subresourceId);

		void AssignSubresourceName(const std::string_view passName, const std::string_view subresourceId, const std::string_view subresourceName);
		void AssignBackbufferName(const std::string_view backbufferName);

		void SetPassSubresourceLayout(const std::string_view passName, const std::string_view subresourceId, VkImageLayout layout);
		void SetPassSubresourceUsage(const std::string_view passName, const std::string_view subresourceId, VkImageUsageFlags usage);
		void SetPassSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId, VkImageAspectFlags aspect);
		void SetPassSubresourceStageFlags(const std::string_view passName, const std::string_view subresourceId, VkPipelineStageFlags stage);
		void SetPassSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId, VkAccessFlags accessFlags);

		const RenderableScene*  GetScene()            const;
		const DeviceParameters* GetDeviceParameters() const;
		const ShaderManager*    GetShaderManager()    const;
		const FrameGraphConfig* GetConfig()           const;

		VkImage              GetRegisteredResource(const std::string_view passName, const std::string_view subresourceId) const;
		VkImageView          GetRegisteredSubresource(const std::string_view passName, const std::string_view subresourceId) const;
		VkImageLayout        GetRegisteredSubresourceLayout(const std::string_view passName, const std::string_view subresourceId) const;
		VkImageUsageFlags    GetRegisteredSubresourceUsage(const std::string_view passName, const std::string_view subresourceId) const;
		VkPipelineStageFlags GetRegisteredSubresourceStageFlags(const std::string_view passName, const std::string_view subresourceId) const;
		VkImageAspectFlags   GetRegisteredSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId) const;
		VkAccessFlags        GetRegisteredSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId) const;

		VkImageLayout        GetPreviousPassSubresourceLayout(const std::string_view passName, const std::string_view subresourceId) const;
		VkImageUsageFlags    GetPreviousPassSubresourceUsage(const std::string_view passName, const std::string_view subresourceId) const;
		VkPipelineStageFlags GetPreviousPassSubresourceStageFlags(const std::string_view passName, const std::string_view subresourceId) const;
		VkImageAspectFlags   GetPreviousPassSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId) const;
		VkAccessFlags        GetPreviousPassSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId) const;

		VkImageLayout        GetNextPassSubresourceLayout(const std::string_view passName, const std::string_view subresourceId) const;
		VkImageUsageFlags    GetNextPassSubresourceUsage(const std::string_view passName, const std::string_view subresourceId) const;
		VkPipelineStageFlags GetNextPassSubresourceStageFlags(const std::string_view passName, const std::string_view subresourceId) const;
		VkImageAspectFlags   GetNextPassSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId) const;
		VkAccessFlags        GetNextPassSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId) const;

		void Build();

	private:
		void BuildAdjacencyList();
		void TopologicalSortNode(std::vector<uint_fast8_t>& visited, std::vector<uint_fast8_t>& onStack, size_t passIndex, std::vector<size_t>& sortedPassIndices);
		
		void BuildPassObjects();

		bool PassesIntersect(const RenderPassName& writingPass, const RenderPassName& readingPass);

	private:
		FrameGraph* mGraphToBuild;

		std::vector<std::unordered_set<size_t>> mAdjacencyList;

		std::vector<RenderPassName>                              mRenderPassNames;
		std::unordered_map<RenderPassName, RenderPassCreateFunc> mRenderPassCreateFunctions;
		std::unordered_map<RenderPassName, uint32_t>             mRenderPassIndices;
                  
		std::unordered_map<RenderPassName, std::unordered_set<SubresourceId>> mRenderPassesReadSubresourceIds;
		std::unordered_map<RenderPassName, std::unordered_set<SubresourceId>> mRenderPassesWriteSubresourceIds;

		std::unordered_map<RenderPassName, std::unordered_map<SubresourceName, SubresourceId>>          mRenderPassesSubresourceNameIds;
		std::unordered_map<RenderPassName, std::unordered_map<SubresourceId, ImageSubresourceMetadata>> mRenderPassesSubresourceMetadatas;

		SubresourceName mBackbufferName;

		//Several things that might be needed to create some of the passes
		const RenderableScene*  mScene;
		const DeviceParameters* mDeviceParameters;
		const ShaderManager*    mShaderManager;
	};
}