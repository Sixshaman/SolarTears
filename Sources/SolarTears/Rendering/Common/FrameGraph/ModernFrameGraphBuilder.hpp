#pragma once

#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include "ModernFrameGraph.hpp"
#include "ModernFrameGraphMisc.hpp"
#include "FrameGraphDescription.hpp"
#include "../../../Core/DataStructures/Span.hpp"

class FrameGraphConfig;

class ModernFrameGraphBuilder
{
protected:
	constexpr static uint32_t TextureFlagAutoBeforeBarrier = 0x01; //A before-barrier is handled by render pass itself
	constexpr static uint32_t TextureFlagAutoAfterBarrier  = 0x02; //An after-barrier is handled by render pass itself

	using PassSubresourceId = uint16_t;

	struct SubresourceMetadataNode
	{
		SubresourceMetadataNode* PrevPassNode;
		SubresourceMetadataNode* NextPassNode;

		//In case of ping-pong or swapchain images the images are stored in a range. ImageIndex and ImageViewIndex point to the first image/view in the range
		uint32_t        FirstFrameHandle;      //The id of the first frame in the list of frame graph textures. Common for all nodes in the resource
		uint32_t        FirstFrameViewHandle;  //The id of the first frame in the frame graph texture views
		uint32_t        FrameCount;            //The number of different frames in the resource. Common for all nodes in the resource
		uint32_t        SubresourceInfoIndex;  //The id of the API-specific subresource data
		uint32_t        Flags;                 //Per-subresource flags
		RenderPassClass PassClass;             //The pass class (Graphics/Compute/Copy) that uses the node

		uint64_t ViewSortKey; //The key to determine unique image views for subresource
	};

	struct TextureResourceCreateInfo
	{
		std::string_view         Name;
		SubresourceMetadataNode* MetadataHead;
	};

	struct TextureSubresourceCreateInfo
	{
		uint32_t SubresourceInfoIndex;
		uint32_t ImageIndex;
		uint32_t ImageViewIndex;
	};

public:
	ModernFrameGraphBuilder(ModernFrameGraph* graphToBuild, FrameGraphDescription&& frameGraphDescription);
	~ModernFrameGraphBuilder();

	void RegisterReadSubresource(const std::string_view passName, const std::string_view subresourceId);
	void RegisterWriteSubresource(const std::string_view passName, const std::string_view subresourceId);

	void EnableSubresourceAutoBeforeBarrier(const std::string_view passName, const std::string_view subresourceId, bool autoBarrier = true);
	void EnableSubresourceAutoAfterBarrier(const std::string_view passName, const std::string_view subresourceId,  bool autoBarrier = true);

	void Build();

	const FrameGraphConfig* GetConfig() const;

private:
	void RegisterRenderPass(RenderPassType passType, const std::string_view passName);

private:
	//Creates present pass
	void CreatePresentPass();

	//Builds frame graph adjacency list
	void BuildAdjacencyList(std::unordered_map<FrameGraphDescription::RenderPassName, std::unordered_set<FrameGraphDescription::RenderPassName>>& adjacencyList);

	//Sorts frame graph passes topologically
	void BuildSortedRenderPassesTopological(const std::unordered_map<FrameGraphDescription::RenderPassName, std::unordered_set<FrameGraphDescription::RenderPassName>>& adjacencyList);

	//Sorts frame graph passes (already sorted topologically) by dependency level
	void BuildSortedRenderPassesDependency(const std::unordered_map<FrameGraphDescription::RenderPassName, std::unordered_set<FrameGraphDescription::RenderPassName>>& adjacencyList);

	//Builds render pass dependency levels
	void BuildDependencyLevels();

	//Changes pass classes according to async compute/transfer use
	void AdjustPassClasses();

	//Validates PrevPassMetadata and NextPassMetadata links in each subresource info
	void ValidateSubresourceLinks();

	//Validates pass classes in each subresource info
	void ValidateSubresourcePassClasses();

	//Finds all passes that use swapchain images. Such passes need to be swapped every frame
	void FindBackbufferPasses(std::unordered_set<FrameGraphDescription::RenderPassName>& swapchainPassNames);

	//Finds own render pass periods, i.e. the minimum number of pass objects required for all possible non-swapchain frame combinations
	void CalculatePassPeriods();

	//Build the render pass objects
	void BuildPassObjects();

	//Build the between-pass barriers
	void BuildBarriers();

	//Build the before- and after-pass barriers
	uint32_t BuildPassBarriers(const FrameGraphDescription::RenderPassName& passName, uint32_t barrierOffset, ModernFrameGraph::BarrierSpan* outBarrierSpan);

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

	//Initializes ImageIndex and ImageViewIndex fields of nodes. Returns image view count written
	uint32_t ValidateImageAndViewIndices(std::vector<TextureResourceCreateInfo>& textureResourceCreateInfos, uint32_t imageIndexOffset);

	//Initializes ImageIndex and ImageViewIndex fields of nodes in a single resource. Returns image view count per resource in a single frame
	void ValidateImageAndViewIndicesInResource(TextureResourceCreateInfo* createInfo, uint32_t imageIndex);

	//Recursively sort subtree topologically
	void TopologicalSortNode(const std::unordered_map<FrameGraphDescription::RenderPassName, std::unordered_set<FrameGraphDescription::RenderPassName>>& adjacencyList, std::unordered_set<FrameGraphDescription::RenderPassName>& visited, std::unordered_set<FrameGraphDescription::RenderPassName>& onStack, const FrameGraphDescription::RenderPassName& renderPassName);

	//Test if two writingPass writes to readingPass
	bool PassesIntersect(const FrameGraphDescription::RenderPassName& writingPass, const FrameGraphDescription::RenderPassName& readingPass);

protected:
	const Span<uint32_t> GetBackbufferImageSpan() const;

protected:
	//Creates a new subresource info record
	virtual uint32_t AddSubresourceMetadata() = 0;

	//Creates a new subresource info record for present pass
	virtual uint32_t AddPresentSubresourceMetadata() = 0;

	//Registers render pass related data in the graph (inputs, outputs, possibly shader bindings, etc)
	virtual void RegisterPassInGraph(RenderPassType passType, const FrameGraphDescription::RenderPassName& passName) = 0;

	//Creates a new render pass
	virtual void CreatePassObject(const FrameGraphDescription::RenderPassName& passName, RenderPassType passType, uint32_t frame) = 0;

	//Gives a free render pass span id
	virtual uint32_t NextPassSpanId() = 0;

	//Propagates info (format, access flags, etc.) from one SubresourceInfo to another. Returns true if propagation succeeded or wasn't needed
	virtual bool ValidateSubresourceViewParameters(SubresourceMetadataNode* currNode, SubresourceMetadataNode* prevNode) = 0;
		
	//Allocates the storage for image views defined by sort keys
	virtual void AllocateImageViews(const std::vector<uint64_t>& sortKeys, uint32_t frameCount, std::vector<uint32_t>& outViewIds) = 0;

	//Creates image objects
	virtual void CreateTextures(const std::vector<TextureResourceCreateInfo>& textureCreateInfos, const std::vector<TextureResourceCreateInfo>& backbufferCreateInfos, uint32_t totalTextureCount) const = 0;

	//Creates image view objects
	virtual void CreateTextureViews(const std::vector<TextureSubresourceCreateInfo>& textureViewCreateInfos) const = 0;

	//Add a barrier to execute before a pass
	virtual uint32_t AddBeforePassBarrier(uint32_t imageIndex, RenderPassClass prevPassClass, uint32_t prevPassSubresourceInfoIndex, RenderPassClass currPassClass, uint32_t currPassSubresourceInfoIndex) = 0;

	//Add a barrier to execute before a pass
	virtual uint32_t AddAfterPassBarrier(uint32_t imageIndex, RenderPassClass currPassClass, uint32_t currPassSubresourceInfoIndex, RenderPassClass nextPassClass, uint32_t nextPassSubresourceInfoIndex) = 0;

	//Initializes command buffer, job info, etc. for the frame graph
	virtual void InitializeTraverseData() const = 0;

	//Get the number of swapchain images
	virtual uint32_t GetSwapchainImageCount() const = 0;

protected:
	ModernFrameGraph* mGraphToBuild;

	FrameGraphDescription mFrameGraphDescription;

	std::vector<FrameGraphDescription::RenderPassName> mSortedRenderPassNames;

	std::unordered_map<RenderPassType, RenderPassClass> mRenderPassClassTable;
	
	std::unordered_map<FrameGraphDescription::RenderPassName, RenderPassClass> mRenderPassClasses;
	std::unordered_map<FrameGraphDescription::RenderPassName, uint32_t>        mRenderPassDependencyLevels;
	std::unordered_map<FrameGraphDescription::RenderPassName, uint32_t>        mRenderPassOwnPeriods;

	std::unordered_map<FrameGraphDescription::RenderPassName, std::unordered_set<FrameGraphDescription::SubresourceId>> mRenderPassesReadSubresourceIds;
	std::unordered_map<FrameGraphDescription::RenderPassName, std::unordered_set<FrameGraphDescription::SubresourceId>> mRenderPassesWriteSubresourceIds;

	std::unordered_map<FrameGraphDescription::RenderPassName, std::unordered_map<FrameGraphDescription::SubresourceId, SubresourceMetadataNode>> mRenderPassesSubresourceMetadatas;
};