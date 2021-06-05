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

	enum class SubresourceFormat: uint32_t
	{
		Rgba8,
		Bgra8,
		Rgba16,
		Rgba32,
		R32,
		D24X8,
		D24S8,
		D32,

		Unknown
	};

	enum class SubresourceViewTypeFlag: uint32_t
	{
		Unknown = 0,

		ShaderResource  = 0x01,
		UnorderedAccess = 0x02,
		RenderTarget    = 0x04,
		DepthStencil    = 0x08,
	};

	struct SubresourceAddress
	{
		RenderPassName PassName;
		SubresourceId  SubresId;
	};

	struct TextureViewInfo
	{
		SubresourceFormat               Format;
		std::vector<SubresourceAddress> ViewAddresses;
	};

	struct TextureResourceCreateInfo
	{
		SubresourceFormat            Format;
		uint32_t                     SubresourceFlags;
		SubresourceName              Name;
		std::vector<TextureViewInfo> ImageViewInfos;
	};

private:
	struct TextureSubresourceMetadataNode
	{
		TextureSubresourceMetadataNode* PrevPassMetadata;
		TextureSubresourceMetadataNode* NextPassMetadata;

		uint32_t MetadataInfoIndex;
	};

	struct TextureSubresourceMetadataInfo
	{
		uint32_t                ImageIndex;
		uint32_t                ImageViewIndex;
		SubresourceViewTypeFlag ViewTypeFlag;
		RenderPassType          PassType;
		SubresourceFormat       Format;
	};
	
	struct BackbufferResourceCreateInfo
	{
		std::vector<TextureViewInfo> ImageViewInfos;
	};

public:
	ModernFrameGraphBuilder(ModernFrameGraph* graphToBuild);
	~ModernFrameGraphBuilder();

	void RegisterReadSubresource(const std::string_view passName, const std::string_view subresourceId);
	void RegisterWriteSubresource(const std::string_view passName, const std::string_view subresourceId);

	void AssignSubresourceName(const std::string_view passName, const std::string_view subresourceId, const std::string_view subresourceName);
	void AssignBackbufferName(const std::string_view backbufferName);

	void Build();

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

	//Create subresources
	void BuildSubresources(std::unordered_set<RenderPassName>& swapchainPassNames);

	//Merges all image view create descriptions with same Format and Aspect Flags into single structures with multiple ViewAddresses
	void MergeImageViewInfos(std::unordered_map<SubresourceName, TextureResourceCreateInfo>& inoutImageResourceCreateInfos);

	//Fixes 0 aspect flags and UNDEFINED formats in subresource metadatas from the information from image create infos
	void PropagateMetadatasFromImageViews(const std::unordered_map<SubresourceName, TextureResourceCreateInfo>& imageCreateInfos, const std::unordered_map<SubresourceName, BackbufferResourceCreateInfo>& backbufferCreateInfos);

	//Creates descriptions for resource creation
	void BuildResourceCreateInfos(std::unordered_map<SubresourceName, TextureResourceCreateInfo>& outImageCreateInfos, std::unordered_map<SubresourceName, BackbufferResourceCreateInfo>& outBackbufferCreateInfos, std::unordered_set<RenderPassName>& swapchainPassNames);

	//Builds flat lists of TextureResourceCreateInfo and TextureViewInfo, indexed with ImageIndex and ImageViewIndex of TextureSubresourceMetadata
	void BuildIndexedFlatCreateInfoLists(const std::unordered_map<SubresourceName, TextureResourceCreateInfo>& imageResourceCreateInfos, std::vector<TextureResourceCreateInfo>& outTextureCreateInfos, std::vector<TextureViewInfo>& outViewInfos);

	//Creates swapchain images and views (non-owning, ping-ponging)
	void CreateSwapchainImageViews(const std::unordered_map<SubresourceName, BackbufferResourceCreateInfo>& backbufferResourceCreateInfos, SubresourceFormat swapchainFormat);

	//Recursively sort subtree topologically
	void TopologicalSortNode(const std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList, std::unordered_set<RenderPassName>& visited, std::unordered_set<RenderPassName>& onStack, const RenderPassName& renderPassName, std::vector<RenderPassName>& sortedPassNames);

	//Test if two writingPass writes to readingPass
	bool PassesIntersect(const RenderPassName& writingPass, const RenderPassName& readingPass);

protected:
	//Creates image objects
	virtual void CreateImages(const std::vector<TextureResourceCreateInfo>& textureCreateInfos) const = 0;

	//Creates image view/descriptor objects
	virtual void CreateImageViews(const std::vector<TextureViewInfo>& viewInfos) const = 0;

protected:
	ModernFrameGraph* mGraphToBuild;

	std::vector<RenderPassName> mRenderPassNames;

	std::unordered_map<RenderPassName, RenderPassType> mRenderPassTypes;
	std::unordered_map<RenderPassName, uint32_t>       mRenderPassDependencyLevels;

	std::unordered_map<RenderPassName, std::unordered_set<SubresourceId>> mRenderPassesReadSubresourceIds;
	std::unordered_map<RenderPassName, std::unordered_set<SubresourceId>> mRenderPassesWriteSubresourceIds;

	std::unordered_map<RenderPassName, std::unordered_map<SubresourceName, SubresourceId>>                  mRenderPassesSubresourceNameIds;
	std::unordered_map<RenderPassName, std::unordered_map<SubresourceId,   TextureSubresourceMetadataNode>> mRenderPassesSubresourceMetadatas;

	std::vector<TextureSubresourceMetadataInfo> mMetadataInfos;

	SubresourceName mBackbufferName;
};