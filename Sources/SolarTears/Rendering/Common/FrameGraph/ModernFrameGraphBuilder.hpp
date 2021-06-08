#pragma once

#include <vector>
#include <string>
#include "ModernRenderPass.hpp"
#include <unordered_set>
#include <unordered_map>

class ModernFrameGraph;

class ModernFrameGraphBuilder
{
protected:
	using RenderPassName  = std::string;
	using SubresourceId   = std::string;
	using SubresourceName = std::string;

	constexpr static std::string_view PresentPassName         = "SPECIAL_PRESENT_ACQUIRE_PASS";
	constexpr static std::string_view BackbufferPresentPassId = "SpecialPresentAcquirePass-Backbuffer";

	struct SubresourceMetadataNode
	{
		SubresourceMetadataNode* PrevPassNode;
		SubresourceMetadataNode* NextPassNode;

		uint32_t       SubresourceInfoIndex;
		uint32_t       ImageIndex;
		uint32_t       ImageViewIndex;
		RenderPassType PassType;
	};

	struct TextureSubresourceInfoSpan
	{
		uint32_t FirstSubresourceInfoIndex;
		uint32_t LastSubresourceInfoIndex;
	};

	struct TextureResourceCreateInfo
	{
		std::string_view           Name;
		SubresourceMetadataNode*   MetadataHead;
		TextureSubresourceInfoSpan SubresourceSpan;
	};

public:
	ModernFrameGraphBuilder(ModernFrameGraph* graphToBuild);
	~ModernFrameGraphBuilder();

	void RegisterReadSubresource(const std::string_view passName, const std::string_view subresourceId);
	void RegisterWriteSubresource(const std::string_view passName, const std::string_view subresourceId);

	void AssignSubresourceName(const std::string_view passName, const std::string_view subresourceId, const std::string_view subresourceName);
	void AssignBackbufferName(const std::string_view backbufferName);

	void Build();

	const FrameGraphConfig* GetConfig() const;

private:
	//Builds frame graph adjacency list
	void BuildAdjacencyList(std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList);

	//Sorts frame graph passes topologically
	void SortRenderPassesTopological(const std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList, std::vector<RenderPassName>& unsortedPasses);

	//Sorts frame graph passes (already sorted topologically) by dependency level
	void SortRenderPassesDependency(const std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList, std::vector<RenderPassName>& topologicallySortedPasses);

	////Builds render pass dependency levels
	void BuildDependencyLevels();

	//Validates PrevPassMetadata and NextPassMetadata links in each subresource info
	void ValidateSubresourceLinks();

	//Validates queue families in each subresource info
	void ValidateSubresourcePassTypes();

	//Finds all passes that use swapchain images. Such passes need to be swapped every frame
	void FindBackbufferPasses(std::unordered_set<RenderPassName>& swapchainPassNames);

	//Create subresources
	void BuildSubresources();

	//Creates descriptions for resource creation
	void BuildResourceCreateInfos(std::vector<TextureResourceCreateInfo>& outTextureCreateInfos, std::vector<TextureResourceCreateInfo>& outBackbufferCreateInfos, std::vector<uint32_t>& outViewSubresourceInfoIndices);

	//Validates all uninitialized parameters in subresource infos, propagating them from passes before
	void PropagateMetadatas(const std::vector<TextureResourceCreateInfo>& textureCreateInfos, const std::vector<TextureResourceCreateInfo>& backbufferCreateInfos);

	//Propagates uninitialized parameters in a single resource
	void PropagateMetadatasInResource(const TextureResourceCreateInfo& createInfo);

	//Initializes SubresourceInfoSpan field of TextureResourceCreateInfo. The span points to a newly created range in viewSubresourceInfoIndices
	void CreateSubresourceInfoSpans(std::vector<TextureResourceCreateInfo>& textureResourceCreateInfos, std::vector<uint32_t>& inoutViewSubresourceInfoIndices);

	//Validates ImageIndex and ImageViewIndex of TextureSubresourceMetadata
	void AssignImageAndViewIndices(std::vector<TextureResourceCreateInfo>& textureCreateInfos, std::vector<TextureResourceCreateInfo>& backbufferCreateInfos);

	//Validates ImageIndex and ImageViewIndex of TextureSubresourceMetadata. Returns the number of different image indices that were assigned
	uint32_t AssignImageAndViewIndicesForResource(const TextureResourceCreateInfo& createInfo, uint32_t imageIndex, uint32_t baseImageViewIndex);

	//Creates swapchain images and views (non-owning, ping-ponging)
	void CreateSwapchainImageViews(const std::unordered_map<SubresourceName, BackbufferResourceCreateInfo>& backbufferResourceCreateInfos);

	//Recursively sort subtree topologically
	void TopologicalSortNode(const std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList, std::unordered_set<RenderPassName>& visited, std::unordered_set<RenderPassName>& onStack, const RenderPassName& renderPassName, std::vector<RenderPassName>& sortedPassNames);

	//Test if two writingPass writes to readingPass
	bool PassesIntersect(const RenderPassName& writingPass, const RenderPassName& readingPass);

protected:
	//Creates a new subresource info record
	virtual uint32_t AddSubresourceMetadata() = 0;

	//Finds all indices in subresourceIndices that correspond to non-unique image views, and creates a list that only points to unique entries
	virtual void BuildUniqueSubresourceList(const std::vector<uint32_t>& subresourceIndices, std::vector<uint32_t>& outUniqueIndices) = 0;

	//Propagates info (format, access flags, etc.) from one SubresourceInfo to another. Returns true if propagation succeeded or wasn't needed
	virtual bool PropagateSubresourceParameters(uint32_t indexFrom, uint32_t indexTo) = 0;

	//Creates image objects
	virtual void CreateTextures(const std::vector<TextureResourceCreateInfo>& textureCreateInfos) const = 0;

	//Creates image view objects
	virtual void CreateTextureViews(const std::vector<TextureResourceCreateInfo>& textureCreateInfos, const std::vector<uint32_t>& subresourceInfoIndices) const = 0;

protected:
	ModernFrameGraph* mGraphToBuild;

	std::vector<RenderPassName> mRenderPassNames;

	std::unordered_map<RenderPassName, RenderPassType> mRenderPassTypes;
	std::unordered_map<RenderPassName, uint32_t>       mRenderPassDependencyLevels;

	std::unordered_map<RenderPassName, std::unordered_set<SubresourceId>> mRenderPassesReadSubresourceIds;
	std::unordered_map<RenderPassName, std::unordered_set<SubresourceId>> mRenderPassesWriteSubresourceIds;

	std::unordered_map<RenderPassName, std::unordered_map<SubresourceName, SubresourceId>>           mRenderPassesSubresourceNameIds;
	std::unordered_map<RenderPassName, std::unordered_map<SubresourceId,   SubresourceMetadataNode>> mRenderPassesSubresourceMetadatas;

	SubresourceName mBackbufferName;
};