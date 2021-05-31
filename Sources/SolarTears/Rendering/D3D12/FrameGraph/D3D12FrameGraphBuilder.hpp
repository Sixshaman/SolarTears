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
		constexpr static uint32_t TextureFlagAutoBarrier       = 0x01; //Barrier is handled by render pass itself
		constexpr static uint32_t TextureFlagStateAutoPromoted = 0x02; //The current resource state was promoted automatically from COMMON

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

	private:
		//Creates descriptions for resource creation
		void BuildResourceCreateInfos(std::unordered_map<SubresourceName, TextureCreateInfo>& outImageCreateInfos, std::unordered_map<SubresourceName, BackbufferCreateInfo>& outBackbufferCreateInfos, std::unordered_set<RenderPassName>& swapchainPassNames);

		//Initializes InitialState and ClearValue fields of texture create infos
		void InitResourceInitialStatesAndClearValues(std::unordered_map<SubresourceName, TextureCreateInfo>& inoutTextureCreateInfos);

		//Clears DENY_SHADER_RESOURCE resource desc flags
		void CleanDenyShaderResourceFlags(std::unordered_map<SubresourceName, TextureCreateInfo>& inoutTextureCreateInfos);

		//Validates StateWasPromoted flag in each subresource info
		void ValidateCommonPromotion();

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