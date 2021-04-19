#pragma once

#include "D3D12RenderPass.hpp"
#include "D3D12FrameGraph.hpp"
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <array>

namespace D3D12
{
	class MemoryManager;
	class ShaderManager;
	class DeviceQueues;
	class DeviceFeatures;

	using RenderPassCreateFunc = std::unique_ptr<RenderPass>(*)(ID3D12Device8*, const FrameGraphBuilder*, const std::string&);

	class FrameGraphBuilder
	{
		using RenderPassName  = std::string;
		using SubresourceId   = std::string;
		using SubresourceName = std::string;

		constexpr static std::string_view PresentPassName         = "SPECIAL_PRESENT_PASS";
		constexpr static std::string_view BackbufferPresentPassId = "SpecialPresentPass-Backbuffer";

		constexpr static uint32_t TextureFlagAutoBarrier       = 0x01; //Barrier is handled by render pass itself
		constexpr static uint32_t TextureFlagStateAutoPromoted = 0x02; //The current resource state was promoted automatically from COMMON
		
		enum SubresourceViewType
		{
			ShaderResource,
			UnorderedAccess,
			RenderTarget,
			DepthStencil,

			Unknown
		};

		struct TextureSubresourceMetadata
		{
			uint64_t MetadataId;

			TextureSubresourceMetadata* PrevPassMetadata;
			TextureSubresourceMetadata* NextPassMetadata;

			uint32_t                ImageIndex;
			uint32_t                SrvUavIndex;
			uint32_t                RtvIndex;
			uint32_t                DsvIndex;
			uint32_t                MetadataFlags;
			D3D12_COMMAND_LIST_TYPE QueueOwnership;
			DXGI_FORMAT             Format;
			D3D12_RESOURCE_STATES   ResourceState;
		};

		struct SubresourceAddress
		{
			RenderPassName PassName;
			SubresourceId  SubresId;
		};

		struct TextureViewInfo
		{
			DXGI_FORMAT                     Format;
			SubresourceViewType             ViewType;
			std::vector<SubresourceAddress> ViewAddresses;
		};

		struct TextureCreateInfo
		{
			D3D12_RESOURCE_DESC1         ResourceDesc;
			D3D12_RESOURCE_STATES        InitialState;
			D3D12_CLEAR_VALUE            OptimizedClearValue;
			std::vector<TextureViewInfo> ViewInfos;
		};

		struct BackbufferCreateInfo
		{
			std::vector<TextureViewInfo> ViewInfos;
		};

	public:
		FrameGraphBuilder(FrameGraph* graphToBuild, const RenderableScene* scene, const DeviceFeatures* deviceFeatures, const ShaderManager* shaderManager);
		~FrameGraphBuilder();

		void RegisterRenderPass(const std::string_view passName, RenderPassCreateFunc createFunc, RenderPassType passType);
		void RegisterReadSubresource(const std::string_view passName, const std::string_view subresourceId);
		void RegisterWriteSubresource(const std::string_view passName, const std::string_view subresourceId);

		void AssignSubresourceName(const std::string_view passName, const std::string_view subresourceId, const std::string_view subresourceName);
		void AssignBackbufferName(const std::string_view backbufferName);

		void SetPassSubresourceFormat(const std::string_view passName, const std::string_view subresourceId, DXGI_FORMAT format);
		void SetPassSubresourceState(const std::string_view passName,  const std::string_view subresourceId, D3D12_RESOURCE_STATES state);

		void EnableSubresourceAutoBarrier(const std::string_view passName, const std::string_view subresourceId, bool autoBaarrier = true);

		const RenderableScene*  GetScene()          const;
		const DeviceFeatures*   GetDeviceFeatures() const;
		const ShaderManager*    GetShaderManager()  const;
		const FrameGraphConfig* GetConfig()         const;

		ID3D12Resource2*            GetRegisteredResource(const std::string_view passName,          const std::string_view subresourceId) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetRegisteredSubresourceSrvUav(const std::string_view passName, const std::string_view subresourceId) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetRegisteredSubresourceRtv(const std::string_view passName,    const std::string_view subresourceId) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetRegisteredSubresourceDsv(const std::string_view passName,    const std::string_view subresourceId) const;
		DXGI_FORMAT                 GetRegisteredSubresourceFormat(const std::string_view passName, const std::string_view subresourceId) const;
		D3D12_RESOURCE_STATES       GetRegisteredSubresourceState(const std::string_view passName,  const std::string_view subresourceId) const;

		D3D12_RESOURCE_STATES GetPreviousPassSubresourceState(const std::string_view passName, const std::string_view subresourceId) const;
		D3D12_RESOURCE_STATES GetNextPassSubresourceState(const std::string_view passName,     const std::string_view subresourceId) const;

		D3D12_CPU_DESCRIPTOR_HANDLE GetFrameGraphSrvHeapStart() const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetFrameGraphRtvHeapStart() const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetFrameGraphDsvHeapStart() const;

		void Build(ID3D12Device8* device, const MemoryManager* memoryAllocator, const SwapChain* swapChain);

	private:
		//Builds frame graph adjacency list
		void BuildAdjacencyList(std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList);

		//Sorts frame graph passes topologically
		void SortRenderPassesTopological(const std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList, std::vector<RenderPassName>& unsortedPasses);

		//Sorts frame graph passes (already sorted topologically) by dependency level
		void SortRenderPassesDependency(const std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList, std::vector<RenderPassName>& topologicallySortedPasses);

		////Builds render pass dependency levels
		void BuildDependencyLevels();

		//Recursively sort subtree topologically
		void TopologicalSortNode(const std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList, std::unordered_set<RenderPassName>& visited, std::unordered_set<RenderPassName>& onStack, const RenderPassName& renderPassName, std::vector<RenderPassName>& sortedPassNames);

		//Creates descriptions for resource creation
		void BuildResourceCreateInfos(std::unordered_map<SubresourceName, TextureCreateInfo>& outImageCreateInfos, std::unordered_map<SubresourceName, BackbufferCreateInfo>& outBackbufferCreateInfos, std::unordered_set<RenderPassName>& swapchainPassNames);

		//Merges all image view create descriptions with same Format and Aspect Flags into single structures with multiple ViewAddresses
		void MergeImageViewInfos(std::unordered_map<SubresourceName, TextureCreateInfo>& inoutTextureCreateInfos);

		//Initializes InitialState and ClearValue fields of texture create infos
		void InitResourceInitialStatesAndClearValues(std::unordered_map<SubresourceName, TextureCreateInfo>& inoutTextureCreateInfos);

		//Validates PrevPassMetadata and NextPassMetadata links in each subresource info
		void ValidateSubresourceLinks();

		//Validates queue families in each subresource info
		void ValidateSubresourceCommandListTypes();

		//Validates StateWasPromoted flag in each subresource info
		void ValidateCommonPromotion();

		//Fixes 0 aspect flags and UNDEFINED formats in subresource metadatas from the information from image create infos
		void PropagateMetadatasFromImageViews(const std::unordered_map<SubresourceName, TextureCreateInfo>& imageCreateInfos, const std::unordered_map<SubresourceName, BackbufferCreateInfo>& backbufferCreateInfos);

		//Functions for creating frame graph passes and resources
		void BuildSubresources(ID3D12Device8* device, const MemoryManager* memoryAllocator, const std::vector<ID3D12Resource2*>& swapchainTextures, std::unordered_set<RenderPassName>& swapchainPassNames);
		void BuildPassObjects(ID3D12Device8* device, const std::unordered_set<RenderPassName>& swapchainPassNames);

		//Build resource barriers
		void BuildBarriers();

		//Functions for creating actual frame graph resources/subresources
		void SetSwapchainTextures(const std::unordered_map<SubresourceName, BackbufferCreateInfo>& backbufferResourceCreateInfos, const std::vector<ID3D12Resource2*>& swapchainTextures);
		void CreateTextures(ID3D12Device8* device, const std::unordered_map<SubresourceName, TextureCreateInfo>& imageCreateInfos, const MemoryManager* memoryAllocator);

		//Create descriptors and descriptor heaps
		void CreateDescriptors(ID3D12Device8* device, const std::unordered_map<SubresourceName, TextureCreateInfo>& imageCreateInfos);

		//Test if two writingPass writes to readingPass
		bool PassesIntersect(const RenderPassName& writingPass, const RenderPassName& readingPass);

		//Converts per-pass resource usage to the view type
		SubresourceViewType ViewTypeFromResourceState(D3D12_RESOURCE_STATES resourceState);

		//Set object name for debug messages
		void SetDebugObjectName(ID3D12Resource2* texture, const SubresourceName& name) const;

	private:
		FrameGraph* mGraphToBuild;

		std::vector<RenderPassName>                                 mRenderPassNames;
		std::unordered_map<RenderPassName, RenderPassCreateFunc>    mRenderPassCreateFunctions;
		std::unordered_map<RenderPassName, RenderPassType>          mRenderPassTypes;
		std::unordered_map<RenderPassName, uint32_t>                mRenderPassDependencyLevels;
		std::unordered_map<RenderPassName, D3D12_COMMAND_LIST_TYPE> mRenderPassCommandListTypes;

		std::unordered_map<RenderPassName, std::unordered_set<SubresourceId>> mRenderPassesReadSubresourceIds;
		std::unordered_map<RenderPassName, std::unordered_set<SubresourceId>> mRenderPassesWriteSubresourceIds;

		std::unordered_map<RenderPassName, std::unordered_map<SubresourceName, SubresourceId>>              mRenderPassesSubresourceNameIds;
		std::unordered_map<RenderPassName, std::unordered_map<SubresourceId,   TextureSubresourceMetadata>> mRenderPassesSubresourceMetadatas;

		uint64_t mSubresourceMetadataCounter;

		SubresourceName mBackbufferName;

		UINT mSrvUavCbvDescriptorSize;
		UINT mRtvDescriptorSize;
		UINT mDsvDescriptorSize;

		//Several things that might be needed to create some of the passes
		const RenderableScene* mScene;
		const DeviceFeatures*  mDeviceFeatures;
		const ShaderManager*   mShaderManager;
	};
}