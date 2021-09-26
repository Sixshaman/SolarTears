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
		uint32_t OwnPeriod;       //The period of the pass' own subresources, excluding swapchain images

		Span<uint32_t> SubresourceMetadataSpan; //The indices of pass subresource metadatas
	};

	//Describes a single resource
	struct ResourceMetadata
	{
		ResourceName Name; //The name of the resource

		uint32_t HeadNodeIndex;    //The index of the head SubresourceMetadataNode
		uint32_t FirstFrameHandle; //The id of the first frame of the resource in the frame graph texture list
		uint32_t FrameCount;       //The number of different frames in the resource
	};

	//Describes a particular pass usage of the resource
	struct SubresourceMetadataNode
	{
		uint32_t PrevPassNodeIndex;     //The index of the same resources metadata used in the previous pass
		uint32_t NextPassNodeIndex;     //The index of the same resources metadata used in the next pass
		uint32_t ResourceMetadataIndex; //The index of ResourceMetadata associated with the subresource

		uint32_t        FirstFrameViewHandle; //The id of the first frame of the subresource in the frame graph texture view list
		RenderPassClass PassClass;            //The pass class (Graphics/Compute/Copy) that uses the node
	};

public:
	ModernFrameGraphBuilder(ModernFrameGraph* graphToBuild);
	~ModernFrameGraphBuilder();

	const FrameGraphConfig* GetConfig() const;

	void Build(FrameGraphDescription&& frameGraphDescription);

private:
	//Fills the initial pass info, sorted by dependency level and by topological order
	void RegisterAndSortPasses(const std::unordered_map<RenderPassName, RenderPassType>& renderPassTypes, const std::vector<SubresourceNamingInfo>& subresourceNames, const ResourceName& backbufferName);

	//Fills the initial pass info from the description data
	void InitPassList(const std::unordered_map<RenderPassName, RenderPassType>& renderPassTypes);

	//Fills the initial pass info from the description data
	void InitSubresourceList(const std::vector<SubresourceNamingInfo>& subresourceNames, const ResourceName& backbufferName);

	//Creates lists of written subresource names and read subresource names for each pass
	void BuildReadWriteSubresourceSpans(std::span<const RenderPassName> passes, std::unordered_map<RenderPassName, std::span<std::string_view>>& outReadSpans, std::unordered_map<RenderPassName, std::span<std::string_view>>& outWriteSpans, std::vector<std::string_view>& outNamesFlat);

	//Builds frame graph adjacency list
	void BuildAdjacencyList(const std::unordered_map<RenderPassName, std::span<std::string_view>>& sortedReadNameSpans, const std::unordered_map<RenderPassName, std::span<std::string_view>>& sortedwriteNameSpans, std::unordered_map<RenderPassName, std::span<RenderPassName>>& outAdjacencyList, std::vector<RenderPassName>& outPassNamesFlat);

	//Sorts frame graph passes topologically
	void BuildSortedRenderPassesTopological(std::span<const RenderPassName> passes, const std::unordered_map<RenderPassName, std::span<RenderPassName>>& adjacencyList);

	//Sorts frame graph passes (already sorted topologically) by dependency level
	void BuildSortedRenderPassesDependency(const std::unordered_map<RenderPassName, std::span<RenderPassName>>& adjacencyList);

	//Builds render pass dependency levels
	void BuildDependencyLevels();

	//Changes pass classes according to async compute/transfer use
	void AdjustPassClasses();

	//Validates PrevPassMetadata and NextPassMetadata links in each subresource info
	void ValidateSubresourceLinks();

	//Propagates pass classes in each subresource info
	void PropagateSubresourcePassClasses();

	//Finds all passes that use swapchain images. Such passes need to be swapped every frame
	void FindBackbufferPasses(std::unordered_set<RenderPassName>& swapchainPassNames);

	//Finds own render pass periods, i.e. the minimum number of pass objects required for all possible non-swapchain frame combinations
	void CalculatePassPeriods();

	//Build the render pass objects
	void BuildPassObjects();

	//Build the between-pass barriers
	void BuildBarriers();

	//Build the before- and after-pass barriers
	uint32_t BuildPassBarriers(const RenderPassName& passName, uint32_t barrierOffset, ModernFrameGraph::BarrierSpan* outBarrierSpan);

	//Create subresources
	void BuildSubresources();

	//Creates descriptions for resource creation
	void BuildResourceCreateInfos(std::vector<TextureResourceCreateInfo>& outTextureCreateInfos, std::vector<TextureResourceCreateInfo>& outBackbufferCreateInfos);

	//Creates descriptions for subresource creation
	void BuildSubresourceCreateInfos(const std::vector<TextureResourceCreateInfo>& textureCreateInfos, std::vector<TextureSubresourceCreateInfo>& outTextureViewCreateInfos);

	//Validates all uninitialized parameters in subresource infos, propagating them from passes before
	void PropagateMetadatas(const std::vector<TextureResourceCreateInfo>& textureCreateInfos, const std::vector<TextureResourceCreateInfo>& backbufferCreateInfos);

	//Propagates uninitialized parameters in a single resource
	void PropagateMetadatasInResource(const TextureResourceCreateInfo& createInfo);

	//Validates the location for each resource and subresource. Returns the total count of resources
	uint32_t PrepareResourceLocations(std::vector<TextureResourceCreateInfo>& textureCreateInfos, std::vector<TextureResourceCreateInfo>& backbufferCreateInfos);

	//Recursively sort subtree topologically
	void TopologicalSortNode(const std::unordered_map<RenderPassName, std::span<RenderPassName>>& adjacencyList, std::unordered_set<RenderPassName>& visited, std::unordered_set<RenderPassName>& onStack, const RenderPassName& renderPassName);

	//Test if readSortedSubresourceNames intersects with writeSortedSubresourceNames, assuming both spans are sorted lexicographically
	bool PassesIntersect(const std::span<std::string_view> readSortedSubresourceNames, const std::span<std::string_view> writeSortedSubresourceNames);

protected:
	const Span<uint32_t> GetBackbufferImageSpan() const;

protected:
	//Registers subresource api-specific metadata
	virtual void InitMetadataPayloads() = 0;

	//Checks if the usage of the subresource with subresourceInfoIndex includes reading
	virtual bool IsReadSubresource(uint32_t subresourceInfoIndex) = 0;

	//Checks if the usage of the subresource with subresourceInfoIndex includes writing
	virtual bool IsWriteSubresource(uint32_t subresourceInfoIndex) = 0;

	//Creates a new render pass
	virtual void CreatePassObject(const RenderPassName& passName, RenderPassType passType, uint32_t frame) = 0;

	//Gives a free render pass span id
	virtual uint32_t NextPassSpanId() = 0;

	//Creates image objects
	virtual void CreateTextures() = 0;

	//Creates image view objects
	virtual void CreateTextureViews() = 0;

	//Add a barrier to execute before a pass
	virtual uint32_t AddBeforePassBarrier(uint32_t metadataIndex) = 0;

	//Add a barrier to execute before a pass
	virtual uint32_t AddAfterPassBarrier(uint32_t metadataIndex) = 0;

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