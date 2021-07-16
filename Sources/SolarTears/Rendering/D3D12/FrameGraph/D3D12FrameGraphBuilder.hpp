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

	using RenderPassCreateFunc = std::unique_ptr<RenderPass>(*)(ID3D12Device8*, const FrameGraphBuilder*, const std::string&, uint32_t);
	using RenderPassAddFunc    = void(*)(FrameGraphBuilder* frameGraphBuilder, const std::string& passName);

	class FrameGraphBuilder final: public ModernFrameGraphBuilder
	{
		struct SubresourceInfo
		{
			DXGI_FORMAT           Format;
			D3D12_RESOURCE_STATES State;
			bool                  BarrierPromotedFromCommon;
		};

	public:
		FrameGraphBuilder(FrameGraph* graphToBuild, FrameGraphDescription&& frameGraphDescription, const SwapChain* swapChain);
		~FrameGraphBuilder();

		void SetPassSubresourceFormat(const std::string_view passName, const std::string_view subresourceId, DXGI_FORMAT format);
		void SetPassSubresourceState(const std::string_view passName,  const std::string_view subresourceId, D3D12_RESOURCE_STATES state);

		const ShaderManager* GetShaderManager() const;

		ID3D12Resource2*            GetRegisteredResource(const std::string_view passName,          const std::string_view subresourceId, uint32_t frame) const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetRegisteredSubresourceSrvUav(const std::string_view passName, const std::string_view subresourceId, uint32_t frame) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetRegisteredSubresourceRtv(const std::string_view passName,    const std::string_view subresourceId, uint32_t frame) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetRegisteredSubresourceDsv(const std::string_view passName,    const std::string_view subresourceId, uint32_t frame) const;

		DXGI_FORMAT           GetRegisteredSubresourceFormat(const std::string_view passName, const std::string_view subresourceId) const;
		D3D12_RESOURCE_STATES GetRegisteredSubresourceState(const std::string_view passName,  const std::string_view subresourceId) const;

		D3D12_RESOURCE_STATES GetPreviousPassSubresourceState(const std::string_view passName, const std::string_view subresourceId) const;
		D3D12_RESOURCE_STATES GetNextPassSubresourceState(const std::string_view passName,     const std::string_view subresourceId) const;

		D3D12_GPU_DESCRIPTOR_HANDLE GetFrameGraphSrvHeapStart() const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetFrameGraphRtvHeapStart() const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetFrameGraphDsvHeapStart() const;

		void Build(ID3D12Device8* device, const ShaderManager* shaderManager, const MemoryManager* memoryManager);

	private:
		//Adds a render pass of type Pass to the frame graph pass table
		template<typename Pass>
		void AddPassToTable();

		//Initializes frame graph pass table
		void InitPassTable();

		D3D12_COMMAND_LIST_TYPE PassClassToListType(RenderPassClass passType);

	private:
		//Creates a new subresource info record
		uint32_t AddSubresourceMetadata() override final;

		//Creates a new subresource info record for present pass
		uint32_t AddPresentSubresourceMetadata() override final;

		//Registers render pass inputs and outputs
		void RegisterPassSubresources(RenderPassType passType, const FrameGraphDescription::RenderPassName& passName) override;

		//Creates a new render pass
		void CreatePassObject(const FrameGraphDescription::RenderPassName& passName, RenderPassType passType, uint32_t frame) override final;

		//Gives a free render pass span id
		uint32_t NextPassSpanId() override final;

		//Propagates subresource info (format, access flags, etc.) to a node from the previous one. Also initializes view key. Returns true if propagation succeeded or wasn't needed
		bool ValidateSubresourceViewParameters(SubresourceMetadataNode* currNode, SubresourceMetadataNode* prevNode) override final;

		//Allocates the storage for image views defined by sort keys
		void AllocateImageViews(const std::vector<uint64_t>& sortKeys, uint32_t frameCount, std::vector<uint32_t>& outViewIds) override final;

		//Creates image objects
		void CreateTextures(const std::vector<TextureResourceCreateInfo>& textureCreateInfos, const std::vector<TextureResourceCreateInfo>& backbufferCreateInfos, uint32_t totalTextureCount) const override final;

		//Creates image view objects
		void CreateTextureViews(const std::vector<TextureSubresourceCreateInfo>& textureViewCreateInfos) const override final;

		//Add a barrier to execute before a pass
		uint32_t AddBeforePassBarrier(uint32_t imageIndex, RenderPassClass prevPassClass, uint32_t prevPassSubresourceInfoIndex, RenderPassClass currPassClass, uint32_t currPassSubresourceInfoIndex) override final;

		//Add a barrier to execute before a pass
		uint32_t AddAfterPassBarrier(uint32_t imageIndex, RenderPassClass currPassClass, uint32_t currPassSubresourceInfoIndex, RenderPassClass nextPassClasse, uint32_t nextPassSubresourceInfoIndex) override final;

		//Initializes per-traverse command buffer info
		void InitializeTraverseData() const override final;

		//Get the number of swapchain images
		uint32_t GetSwapchainImageCount() const override final;

	private:
		FrameGraph* mD3d12GraphToBuild;

		std::unordered_map<RenderPassType, RenderPassAddFunc>    mPassAddFuncTable;
		std::unordered_map<RenderPassType, RenderPassCreateFunc> mPassCreateFuncTable;

		std::vector<SubresourceInfo> mSubresourceInfos;

		UINT mSrvUavCbvDescriptorCount;
		UINT mRtvDescriptorCount;
		UINT mDsvDescriptorCount;

		UINT mSrvUavCbvDescriptorSize;
		UINT mRtvDescriptorSize;
		UINT mDsvDescriptorSize;

		//Several things that might be needed to create some of the passes
		ID3D12Device8*         mDevice;
		const MemoryManager*   mMemoryAllocator;
		const SwapChain*       mSwapChain;
		const ShaderManager*   mShaderManager;
	};
}

#include "D3D12FrameGraphBuilder.inl"