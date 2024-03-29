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

		TextureSourceType SourceType;    //Defines ownership of the texture (frame graph, swapchain, etc) 
		uint32_t          HeadNodeIndex; //The index of the head SubresourceMetadataNode
	};

	//Describes a particular pass usage of the resource
	struct SubresourceMetadataNode
	{
		uint32_t PrevPassNodeIndex;     //The index of the same resources metadata used in the previous pass
		uint32_t NextPassNodeIndex;     //The index of the same resources metadata used in the next pass
		uint32_t ResourceMetadataIndex; //The index of ResourceMetadata associated with the subresource

		RenderPassClass PassClass; //The pass class (Graphics/Compute/Copy) that uses the node

#if defined(DEBUG) || defined(_DEBUG)
		std::string_view PassName;
		std::string_view ResourceName;
#endif
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
	void AssignDependencyLevels(const std::vector<uint32_t>& passIndexRemap, const std::vector<std::span<uint32_t>>& oldAdjacencyList);

	//Sorts frame graph passes topologically
	void SortRenderPassesByTopology(const std::vector<std::span<uint32_t>>& unsortedPassAdjacencyList, std::vector<uint32_t>& outPassIndexRemap);

	//Recursively sort subtree topologically
	void TopologicalSortNode(uint32_t passIndex, const std::vector<std::span<uint32_t>>& unsortedPassAdjacencyList, std::vector<uint8_t>& inoutTraversalMarkFlags, std::vector<uint32_t>& inoutPassIndexRemap);

	//Sorts frame graph passes (already sorted topologically) by dependency level
	void SortRenderPassesByDependency();

	//Initializes additional pass data (pass classes, pass frame spans)
	void InitAugmentedPassData();

	//Changes pass classes according to async compute/transfer use
	void AdjustPassClasses();

	//Builds pass spans for each dependency level
	void BuildPassSpans();

	//Validates PrevPassMetadata and NextPassMetadata links in each subresource info
	void ValidateSubresourceLinkedLists();

	//Creates multiple copies of passes and resources, one for each separate frame
	void AmplifyResourcesAndPasses();

	//Helper function to find how many per-frame copies of pass needed, and what is a swap behaviour for the copies
	void FindFrameCountAndSwapType(const std::vector<Span<uint32_t>>& resourceFrameSpans, std::span<const SubresourceMetadataNode> oldPassSubresourceMetadataSpan, uint32_t* outFrameCount, RenderPassFrameSwapType* outSwapType);

	//Helper function to find the correct amplified index of the previous pass
	uint32_t CalculatePrevPassFrameIndex(uint32_t prevPassNonAmplifiedIndex, uint32_t currPassNonAmplifiedIndex, uint32_t currPassFrameIndex);

	//Helper function to find the correct amplified index of the next pass
	uint32_t CalculateNextPassFrameIndex(uint32_t nextPassNonAmplifiedIndex, uint32_t currPassNonAmplifiedIndex, uint32_t currPassFrameIndex);

	//Allocates a span of helper subresource nodes
	Span<uint32_t> AllocateHelperSubresourceSpan(uint32_t templateSubresourceIndex, uint32_t helperNodesNeeded);

	//Initializes additional resource and subresource data (subresource head nodes, subresource payloads)
	void InitAugmentedResourceData();

	//Initializes HeadNodeIndex of resources
	void InitializeHeadNodes();

	//Propagates API-specific subresource data
	void PropagateSubresourcePayloadData();

	//Creates textures and views
	void BuildResources();

	//Build the between-pass barriers
	void BuildBarriers();

protected:
	//Registers subresource api-specific metadata
	virtual void InitMetadataPayloads() = 0;

	//Propagates API-specific subresource data from one subresource to another, within a single resource
	virtual bool PropagateSubresourcePayloadDataVertically(const ResourceMetadata& resourceMetadata) = 0;

	//Propagates API-specific subresource data from one subresource to another, within a single pass
	virtual bool PropagateSubresourcePayloadDataHorizontally(const PassMetadata& passMetadata) = 0;

	//Creates image objects
	virtual void CreateTextures() = 0;

	//Creates image view objects
	virtual void CreateTextureViews() = 0;

	//Creates render pass objects
	virtual void BuildPassObjects() = 0;

	//Create barriers to execute before a pass
	virtual void CreateBeforePassBarriers(const PassMetadata& passMetadata, uint32_t barrierSpanIndex) = 0;

	//Create barriers to execute after a pass
	virtual void CreateAfterPassBarriers(const PassMetadata& passMetadata, uint32_t barrierSpanIndex) = 0;

	//Get the number of swapchain images
	virtual uint32_t GetSwapchainImageCount() const = 0;

protected:
	ModernFrameGraph* mGraphToBuild;

	std::vector<SubresourceMetadataNode> mSubresourceMetadataNodesFlat;
	std::vector<PassMetadata>            mTotalPassMetadatas;
	std::vector<ResourceMetadata>        mResourceMetadatas;

	Span<uint32_t> mRenderPassMetadataSpan;
	Span<uint32_t> mPresentPassMetadataSpan;

	//The sole purpose of helper subresource spans for each subresources is to connect PrevPassNodeIndex and NextPassNodeIndex in multi-frame scenarios
	Span<uint32_t>              mPrimarySubresourceNodeSpan;
	std::vector<Span<uint32_t>> mHelperNodeSpansPerPassSubresource;
};