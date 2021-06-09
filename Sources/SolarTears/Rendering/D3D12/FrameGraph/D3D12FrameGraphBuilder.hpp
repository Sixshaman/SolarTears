#pragma once

#include "D3D12RenderPass.hpp"
#include "D3D12FrameGraph.hpp"
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <array>
#include "../../Common/FrameGraph/ModernFrameGraphBuilder.hpp"

namespace D3D12
{
	class MemoryManager;
	class ShaderManager;
	class DeviceQueues;
	class DeviceFeatures;

	using RenderPassCreateFunc = std::unique_ptr<RenderPass>(*)(ID3D12Device8*, const FrameGraphBuilder*, const std::string&);

	class FrameGraphBuilder: public ModernFrameGraphBuilder
	{
		constexpr static uint32_t TextureFlagAutoBarrier       = 0x01; //Barrier is handled by render pass itself
		constexpr static uint32_t TextureFlagStateAutoPromoted = 0x02; //The current resource state was promoted automatically from COMMON

		struct SubresourceInfo
		{
			DXGI_FORMAT           Format;
			D3D12_RESOURCE_STATES State;
			uint32_t              DescriptorHeapIndex;
			uint32_t              Flags;
		};

	public:
		FrameGraphBuilder(FrameGraph* graphToBuild, const RenderableScene* scene, const DeviceFeatures* deviceFeatures, const ShaderManager* shaderManager);
		~FrameGraphBuilder();

		void RegisterRenderPass(const std::string_view passName, RenderPassCreateFunc createFunc, RenderPassType passType);

		void SetPassSubresourceFormat(const std::string_view passName, const std::string_view subresourceId, DXGI_FORMAT format);
		void SetPassSubresourceState(const std::string_view passName,  const std::string_view subresourceId, D3D12_RESOURCE_STATES state);

		void EnableSubresourceAutoBarrier(const std::string_view passName, const std::string_view subresourceId, bool autoBarrier = true);

		const RenderableScene*  GetScene()          const;
		const DeviceFeatures*   GetDeviceFeatures() const;
		const ShaderManager*    GetShaderManager()  const;

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
		//Validates StateWasPromoted flag in each subresource info
		void ValidateCommonPromotion();

		//Functions for creating frame graph passes and resources
		void BuildPassObjects(ID3D12Device8* device, const std::unordered_set<RenderPassName>& swapchainPassNames);

		//Build resource barriers
		void BuildBarriers();

		//Create descriptors and descriptor heaps
		void CreateDescriptors(ID3D12Device8* device, const std::unordered_map<SubresourceName, TextureCreateInfo>& imageCreateInfos);

		//Set object name for debug messages
		void SetDebugObjectName(ID3D12Resource2* texture, const SubresourceName& name) const;

	private:
		//Creates a new subresource info record
		uint32_t AddSubresourceMetadata() override;

		//Finds all indices in subresourceInfoIndices that refer to non-unique entries in mSubresourceInfos
		//Returns a list of indices pointing to unique entries. Also builds a map to match the old list to the new list
		void BuildUniqueSubresourceList(const std::vector<uint32_t>& subresourceInfoIndices, std::vector<uint32_t>& outUniqueSubresourceInfoIndices, std::vector<uint32_t>& outNewIndexMap) override;

		//Propagates info (format, access flags, etc.) from one SubresourceInfo to another. Returns true if propagation succeeded or wasn't needed
		bool PropagateSubresourceParameters(uint32_t indexFrom, uint32_t indexTo) override;

		//Creates image objects
		void CreateTextures(const std::vector<TextureResourceCreateInfo>& textureCreateInfos) const override;

		//Allocates the storage for image views
		void AllocateTextureViews(size_t textureViewCount);

		//Creates image view objects
		void CreateTextureViews(const std::vector<TextureResourceCreateInfo>& textureCreateInfos, const std::vector<uint32_t>& subresourceInfoIndices) const override;

		//Initializes backbuffer-related 
		uint32_t AllocateBackbufferResources() const override;

	private:
		FrameGraph* mD3d12GraphToBuild;

		std::unordered_map<RenderPassName, RenderPassCreateFunc> mRenderPassCreateFunctions;

		std::vector<SubresourceInfo> mSubresourceInfos;

		UINT mSrvUavCbvDescriptorSize;
		UINT mRtvDescriptorSize;
		UINT mDsvDescriptorSize;

		//Several things that might be needed to create some of the passes
		const RenderableScene* mScene;
		const DeviceFeatures*  mDeviceFeatures;
		const ShaderManager*   mShaderManager;
	};
}