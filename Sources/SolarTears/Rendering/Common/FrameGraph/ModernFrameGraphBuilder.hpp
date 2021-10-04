#pragma once

#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include "ModernFrameGraph.hpp"
#include "ModernFrameGraphMisc.hpp"
#include "FrameGraphDescription.hpp"
#include "../../../Core/DataStructures/Span.hpp"
#include <span>

class FrameGraphConfig;

class ModernFrameGraphBuilder
{
protected:
	//Describes a single render pass
	struct PassMetadata
	{
		RenderPassName Name; //The name of the pass

		RenderPassClass Class; //The class (graphics/compute/copy) of the pass
		RenderPassType  Type;  //The type of the pass

		uint32_t DependencyLevel; //The dependendency level of the pass

		Span<uint32_t> SubresourceMetadataSpan; //The indices of pass subresource metadatas
	};

	//Describes a single resource
	struct ResourceMetadata
	{
		ResourceName Name; //The name of the resource

		uint32_t HeadNodeIndex; //The index of the head SubresourceMetadataNode
		uint32_t ImageHandle;   //The id of the resource in the frame graph texture lis
	};

	//Describes a particular pass usage of the resource
	struct SubresourceMetadataNode
	{
		uint32_t PrevPassNodeIndex;     //The index of the same resources metadata used in the previous pass
		uint32_t NextPassNodeIndex;     //The index of the same resources metadata used in the next pass
		uint32_t ResourceMetadataIndex; //The index of ResourceMetadata associated with the subresource

		uint32_t        ImageViewHandle; //The id of the subresource in the frame graph texture view list
		RenderPassClass PassClass;       //The pass class (Graphics/Compute/Copy) that uses the node
	};

public:
	ModernFrameGraphBuilder(ModernFrameGraph* graphToBuild);
	~ModernFrameGraphBuilder();

	const FrameGraphConfig* GetConfig() const;

	void Build(FrameGraphDescription&& frameGraphDescription);

private:
	//Fills the initial pass info
	void RegisterPasses(const std::unordered_map<RenderPassName, RenderPassType>& renderPassTypes, const std::vector<SubresourceNamingInfo>& subresourceNames, const ResourceName& backbufferName);

	//Fills the initial pass info from the description data
	void InitPassList(const std::unordered_map<RenderPassName, RenderPassType>& renderPassTypes);

	//Fills the initial pass info from the description data
	void InitSubresourceList(const std::vector<SubresourceNamingInfo>& subresourceNames, const ResourceName& backbufferName);

	//Sorts passes by dependency level and by topological order
	void SortPasses();

	//Creates lists of written resource indices and read subresource indices for each pass. Used to speed up the lookup when creating the adjacency list
	void BuildReadWriteSubresourceSpans(std::vector<std::span<std::uint32_t>>& outReadIndexSpans, std::vector<std::span<std::uint32_t>>& outWriteIndexSpans, std::vector<uint32_t>& outIndicesFlat);

	//Builds frame graph adjacency list
	void BuildAdjacencyList(const std::vector<std::span<uint32_t>>& sortedReadNameSpansPerPass, const std::vector<std::span<uint32_t>>& sortedwriteNameSpansPerPass, std::vector<std::span<uint32_t>>& outAdjacencyList, std::vector<uint32_t>& outAdjacentPassIndicesFlat);

	//Test if sorted spans share any element
	bool SpansIntersect(const std::span<uint32_t> leftSortedSpan, const std::span<uint32_t> rightSortedSpan);

	//Assign dependency levels to the render passes
	void AssignDependencyLevels(const std::vector<std::span<uint32_t>>& adjacencyList);

	//Sorts frame graph passes topologically
	void SortRenderPassesByTopology(const std::vector<std::span<uint32_t>>& unsortedPassAdjacencyList);

	//Recursively sort subtree topologically
	void TopologicalSortNode(uint32_t passIndex, const std::vector<std::span<uint32_t>>& unsortedPassAdjacencyList, std::vector<uint8_t>& inoutTraversalMarkFlags, std::vector<PassMetadata>& inoutSortedPasses);

	//Sorts frame graph passes (already sorted topologically) by dependency level
	void SortRenderPassesByDependency();

	//Fills in augmented render pass data (pass classes, spans, pass periods)
	void InitAugmentedData();

	//Changes pass classes according to async compute/transfer use
	void AdjustPassClasses();

	//Propagates pass classes in each subresource info
	void PropagateSubresourcePassClasses();

	//Builds frame graph pass spans
	void BuildPassSpans();

	//Finds own render pass periods, i.e. the minimum number of pass objects required for all possible non-swapchain frame combinations
	void CalculatePassPeriods();

	//Validates PrevPassMetadata and NextPassMetadata links in each subresource info
	void ValidateSubresourceLinks();

	//Propagates API-specific subresource data
	void PropagateSubresourcePayloadData();

	//Creates textures and views
	void BuildResources();

	//Build the render pass objects
	void BuildPassObjects();

	//Build the between-pass barriers
	void BuildBarriers();

protected:
	const Span<uint32_t> GetBackbufferImageSpan() const;

protected:
	//Registers subresource api-specific metadata
	virtual void InitMetadataPayloads() = 0;

	//Checks if the usage of the subresource with subresourceInfoIndex includes reading
	virtual bool IsReadSubresource(uint32_t subresourceInfoIndex) = 0;

	//Checks if the usage of the subresource with subresourceInfoIndex includes writing
	virtual bool IsWriteSubresource(uint32_t subresourceInfoIndex) = 0;

	//Propagates API-specific subresource data from one subresource to another, within a single resource
	virtual bool PropagateSubresourcePayloadDataVertically(const ResourceMetadata& resourceMetadata) = 0;

	//Propagates API-specific subresource data from one subresource to another, within a single pass
	virtual bool PropagateSubresourcePayloadDataHorizontally(const PassMetadata& passMetadata) = 0;

	//Creates image objects
	virtual void CreateTextures() = 0;

	//Creates image view objects
	virtual void CreateTextureViews() = 0;

	//Creates render pass objects
	virtual void CreateObjectsForPass(uint32_t passMetadataIndex, uint32_t passSwapchainImageCount) = 0;

	//Add barriers to execute before a pass
	virtual void AddBeforePassBarriers(const PassMetadata& passMetadata, uint32_t barrierSpanIndex) = 0;

	//Add barriers to execute before a pass
	virtual void AddAfterPassBarriers(const PassMetadata& passMetadata, uint32_t barrierSpanIndex) = 0;

	//Initializes command buffer, job info, etc. for the frame graph
	virtual void InitializeTraverseData() const = 0;

	//Get the number of swapchain images
	virtual uint32_t GetSwapchainImageCount() const = 0;

protected:
	ModernFrameGraph* mGraphToBuild;

	std::vector<SubresourceMetadataNode> mSubresourceMetadataNodesFlat;
	std::vector<PassMetadata>            mPassMetadatas;
	std::vector<ResourceMetadata>        mResourceMetadatas;

	PassMetadata mPresentPassMetadata;
};