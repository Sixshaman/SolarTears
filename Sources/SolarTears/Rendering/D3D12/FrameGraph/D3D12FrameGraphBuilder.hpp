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

		constexpr static std::string_view AcquirePassName         = "SPECIAL_ACQUIRE_PASS";
		constexpr static std::string_view PresentPassName         = "SPECIAL_PRESENT_PASS";
		constexpr static std::string_view BackbufferAcquirePassId = "SpecialAcquirePass-Backbuffer";
		constexpr static std::string_view BackbufferPresentPassId = "SpecialPresentPass-Backbuffer";

		//enum class ResourceType
		//{
		//	ImageResource,
		//	BackbufferResource
		//};

		constexpr static uint32_t TextureFlagAutoBarrier = 0x01; //Barrier is handled by render pass itself

		struct TextureSubresourceMetadata
		{
			uint64_t MetadataId;

			TextureSubresourceMetadata* PrevPassMetadata;
			TextureSubresourceMetadata* NextPassMetadata;

			uint32_t                ImageIndex;
			uint32_t                ImageViewIndex;
			uint32_t                MetadataFlags;
			D3D12_COMMAND_LIST_TYPE QueueOwnership;
			DXGI_FORMAT             Format;
			D3D12_RESOURCE_STATES   ResourceState;
		};

		//struct SubresourceAddress
		//{
		//	RenderPassName PassName;
		//	SubresourceId  SubresId;
		//};

		//struct ImageViewInfo
		//{
		//	VkFormat                        Format;
		//	VkImageAspectFlags              AspectMask;
		//	std::vector<SubresourceAddress> ViewAddresses;
		//};

		//struct ImageResourceCreateInfo
		//{
		//	VkImageCreateInfo          ImageCreateInfo;
		//	std::vector<ImageViewInfo> ImageViewInfos;
		//};

		//struct BackbufferResourceCreateInfo
		//{
		//	std::vector<ImageViewInfo> ImageViewInfos;
		//};

	public:
		FrameGraphBuilder(FrameGraph* graphToBuild, const RenderableScene* scene, const DeviceFeatures* deviceFeatures, const ShaderManager* shaderManager);
		~FrameGraphBuilder();

		//void RegisterRenderPass(const std::string_view passName, RenderPassCreateFunc createFunc, RenderPassType passType);
		//void RegisterReadSubresource(const std::string_view passName, const std::string_view subresourceId);
		//void RegisterWriteSubresource(const std::string_view passName, const std::string_view subresourceId);

		//void AssignSubresourceName(const std::string_view passName, const std::string_view subresourceId, const std::string_view subresourceName);
		//void AssignBackbufferName(const std::string_view backbufferName);

		//void SetPassSubresourceFormat(const std::string_view passName,      const std::string_view subresourceId, VkFormat format);
		//void SetPassSubresourceLayout(const std::string_view passName,      const std::string_view subresourceId, VkImageLayout layout);
		//void SetPassSubresourceUsage(const std::string_view passName,       const std::string_view subresourceId, VkImageUsageFlags usage);
		//void SetPassSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId, VkImageAspectFlags aspect);
		//void SetPassSubresourceStageFlags(const std::string_view passName,  const std::string_view subresourceId, VkPipelineStageFlags stage);
		//void SetPassSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId, VkAccessFlags accessFlags);

		//void EnableSubresourceAutoBarrier(const std::string_view passName, const std::string_view subresourceId, bool autoBaarrier = true);

		//const RenderableScene*  GetScene()            const;
		//const DeviceParameters* GetDeviceParameters() const;
		//const ShaderManager*    GetShaderManager()    const;
		//const FrameGraphConfig* GetConfig()           const;

		//VkImage              GetRegisteredResource(const std::string_view passName,               const std::string_view subresourceId) const;
		//VkImageView          GetRegisteredSubresource(const std::string_view passName,            const std::string_view subresourceId) const;
		//VkFormat             GetRegisteredSubresourceFormat(const std::string_view passName,      const std::string_view subresourceId) const;
		//VkImageLayout        GetRegisteredSubresourceLayout(const std::string_view passName,      const std::string_view subresourceId) const;
		//VkImageUsageFlags    GetRegisteredSubresourceUsage(const std::string_view passName,       const std::string_view subresourceId) const;
		//VkPipelineStageFlags GetRegisteredSubresourceStageFlags(const std::string_view passName,  const std::string_view subresourceId) const;
		//VkImageAspectFlags   GetRegisteredSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId) const;
		//VkAccessFlags        GetRegisteredSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId) const;

		//VkImageLayout        GetPreviousPassSubresourceLayout(const std::string_view passName,      const std::string_view subresourceId) const;
		//VkImageUsageFlags    GetPreviousPassSubresourceUsage(const std::string_view passName,       const std::string_view subresourceId) const;
		//VkPipelineStageFlags GetPreviousPassSubresourceStageFlags(const std::string_view passName,  const std::string_view subresourceId) const;
		//VkImageAspectFlags   GetPreviousPassSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId) const;
		//VkAccessFlags        GetPreviousPassSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId) const;

		//VkImageLayout        GetNextPassSubresourceLayout(const std::string_view passName,      const std::string_view subresourceId) const;
		//VkImageUsageFlags    GetNextPassSubresourceUsage(const std::string_view passName,       const std::string_view subresourceId) const;
		//VkPipelineStageFlags GetNextPassSubresourceStageFlags(const std::string_view passName,  const std::string_view subresourceId) const;
		//VkImageAspectFlags   GetNextPassSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId) const;
		//VkAccessFlags        GetNextPassSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId) const;

		void Build(const DeviceQueues* deviceQueues, const WorkerCommandLists* workerCommandLists, const MemoryManager* memoryAllocator, const SwapChain* swapChain);

	private:
		//Builds frame graph adjacency list
		void BuildAdjacencyList(std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList);

		//Sorts frame graph passes topologically
		void SortRenderPassesTopological(const std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList, std::vector<RenderPassName>& unsortedPasses);

		//Sorts frame graph passes (already sorted topologically) by dependency level
		void SortRenderPassesDependency(const std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList, std::vector<RenderPassName>& topologicallySortedPasses);

		////Builds render pass dependency levels
		void BuildDependencyLevels(const DeviceQueues* deviceQueues, const SwapChain* swapChain);

		//Recursively sort subtree topologically
		void TopologicalSortNode(const std::unordered_map<RenderPassName, std::unordered_set<RenderPassName>>& adjacencyList, std::unordered_set<RenderPassName>& visited, std::unordered_set<RenderPassName>& onStack, const RenderPassName& renderPassName, std::vector<RenderPassName>& sortedPassNames);

		////Creates descriptions for resource creation
		//void BuildResourceCreateInfos(const std::vector<uint32_t>& defaultQueueIndices, std::unordered_map<SubresourceName, ImageResourceCreateInfo>& outImageCreateInfos, std::unordered_map<SubresourceName, BackbufferResourceCreateInfo>& outBackbufferCreateInfos, std::unordered_set<RenderPassName>& swapchainPassNames);
		
		////Merges all image view create descriptions with same Format and Aspect Flags into single structures with multiple ViewAddresses
		//void MergeImageViewInfos(std::unordered_map<SubresourceName, ImageResourceCreateInfo>& inoutImageResourceCreateInfos);

		//Validates PrevPassMetadata and NextPassMetadata links in each subresource info
		//void ValidateSubresourceLinks();

		////Validates queue families in each subresource info
		//void ValidateSubresourceQueues();

		////Fixes 0 aspect flags and UNDEFINED formats in subresource metadatas from the information from image create infos
		//void PropagateMetadatasFromImageViews(const std::unordered_map<SubresourceName, ImageResourceCreateInfo>& imageCreateInfos, const std::unordered_map<SubresourceName, BackbufferResourceCreateInfo>& backbufferCreateInfos);

		////Functions for creating frame graph passes and resources
		//void BuildSubresources(const MemoryManager* memoryAllocator, const std::vector<VkImage>& swapchainImages, std::unordered_set<RenderPassName>& swapchainPassNames, VkFormat swapchainFormat, uint32_t defaultQueueIndex);
		//void BuildPassObjects(const std::unordered_set<RenderPassName>& swapchainPassNames);

		//Builds barriers
		void BuildBarriers();

		////Build descriptors
		//void BuildDescriptors();

		////Transit images from UNDEFINED to usable state
		//void BarrierImages(const DeviceQueues* deviceQueues, const WorkerCommandBuffers* workerCommandBuffers, uint32_t defaultQueueIndex);

		////Functions for creating actual frame graph resources/subresources
		//void SetSwapchainImages(const std::unordered_map<SubresourceName, BackbufferResourceCreateInfo>& backbufferResourceCreateInfos, const std::vector<VkImage>& swapchainImages);
		//void CreateImages(const std::unordered_map<SubresourceName, ImageResourceCreateInfo>& imageResourceCreateInfos, const MemoryManager* memoryAllocator);
		//void CreateImageViews(const std::unordered_map<SubresourceName, ImageResourceCreateInfo>& imageResourceCreateInfos);
		//void CreateSwapchainImageViews(const std::unordered_map<SubresourceName, BackbufferResourceCreateInfo>& backbufferResourceCreateInfos, VkFormat swapchainFormat);

		////Creates VkImageView from imageViewInfo
		//void CreateImageView(const ImageViewInfo& imageViewInfo, VkImage image, VkFormat defaultFormat, VkImageView* outImageView);

		//Test if two writingPass writes to readingPass
		bool PassesIntersect(const RenderPassName& writingPass, const RenderPassName& readingPass);

		////Set object name for debug messages
		//void SetDebugObjectName(VkImage image, const SubresourceName& name) const;

	private:
		FrameGraph* mGraphToBuild;

		std::vector<RenderPassName>                              mRenderPassNames;
		//std::unordered_map<RenderPassName, RenderPassCreateFunc> mRenderPassCreateFunctions;
		//std::unordered_map<RenderPassName, RenderPassType>       mRenderPassTypes;
		std::unordered_map<RenderPassName, uint32_t>             mRenderPassDependencyLevels;
		std::unordered_map<RenderPassName, D3D12_COMMAND_LIST_TYPE> mRenderPassCommandListTypes;

		std::unordered_map<RenderPassName, std::unordered_set<SubresourceId>> mRenderPassesReadSubresourceIds;
		std::unordered_map<RenderPassName, std::unordered_set<SubresourceId>> mRenderPassesWriteSubresourceIds;

		std::unordered_map<RenderPassName, std::unordered_map<SubresourceName, SubresourceId>>              mRenderPassesSubresourceNameIds;
		std::unordered_map<RenderPassName, std::unordered_map<SubresourceId,   TextureSubresourceMetadata>> mRenderPassesSubresourceMetadatas;

		//uint64_t mSubresourceMetadataCounter;

		//SubresourceName mBackbufferName;

		//Several things that might be needed to create some of the passes
		const RenderableScene* mScene;
		const DeviceFeatures*  mDeviceFeatures;
		const ShaderManager*   mShaderManager;
	};
}