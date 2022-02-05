#pragma once

#include "D3D12RenderPass.hpp"
#include "D3D12FrameGraph.hpp"
#include "D3D12FrameGraphMisc.hpp"
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

	struct FrameGraphBuildInfo
	{
		ID3D12Device8*       Device; 
		const ShaderManager* ShaderMgr;
		const MemoryManager* MemoryAllocator;
	};

	class FrameGraphBuilder final: public ModernFrameGraphBuilder
	{
	public:
		FrameGraphBuilder(FrameGraph* graphToBuild, const SwapChain* swapChain);
		~FrameGraphBuilder();

		ID3D12Device8* GetDevice() const;

		const ShaderManager* GetShaderManager() const;

		ID3D12Resource2*            GetRegisteredResource(uint32_t passIndex,          uint_fast16_t subresourceIndex) const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetRegisteredSubresourceSrvUav(uint32_t passIndex, uint_fast16_t subresourceIndex) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetRegisteredSubresourceRtv(uint32_t passIndex,    uint_fast16_t subresourceIndex) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetRegisteredSubresourceDsv(uint32_t passIndex,    uint_fast16_t subresourceIndex) const;

		DXGI_FORMAT           GetRegisteredSubresourceFormat(uint32_t passIndex, uint_fast16_t subresourceIndex) const;
		D3D12_RESOURCE_STATES GetRegisteredSubresourceState(uint32_t passIndex, uint_fast16_t subresourceIndex)  const;

		D3D12_RESOURCE_STATES GetPreviousPassSubresourceState(uint32_t passIndex, uint_fast16_t subresourceIndex) const;
		D3D12_RESOURCE_STATES GetNextPassSubresourceState(uint32_t passIndex, uint_fast16_t subresourceIndex)     const;

		D3D12_GPU_DESCRIPTOR_HANDLE GetFrameGraphSrvHeapStart() const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetFrameGraphRtvHeapStart() const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetFrameGraphDsvHeapStart() const;

		void Build(FrameGraphDescription&& frameGraphDescription, const FrameGraphBuildInfo& buildInfo);

	private:
		D3D12_COMMAND_LIST_TYPE PassClassToListType(RenderPassClass passType);

	private:
		//Registers subresource api-specific metadata
		void InitMetadataPayloads() override final;

		//Propagates API-specific subresource data from one subresource to another, within a single resource
		bool PropagateSubresourcePayloadDataVertically(const ResourceMetadata& resourceMetadata) override final;

		//Propagates API-specific subresource data from one subresource to another, within a single pass
		bool PropagateSubresourcePayloadDataHorizontally(const PassMetadata& passMetadata) override final;

		//Creates image objects
		void CreateTextures() override final;

		//Creates image view objects
		void CreateTextureViews() override final;

		//Build the render pass objects
		void BuildPassObjects() override final;

		//Adds subresource barriers to execute before a pass
		void CreateBeforePassBarriers(const PassMetadata& passMetadata, uint32_t barrierSpanIndex) override final;

		//Adds subresource barriers to execute after a pass
		void CreateAfterPassBarriers(const PassMetadata& passMetadata, uint32_t barrierSpanIndex) override final;

		//Initializes per-traverse command buffer info
		void InitializeTraverseData() const override final;

		//Get the number of swapchain images
		uint32_t GetSwapchainImageCount() const override final;

	private:
		FrameGraph* mD3d12GraphToBuild;

		std::vector<SubresourceMetadataPayload> mSubresourceMetadataPayloads;

		//Several things that might be needed to create some of the passes
		ID3D12Device8*         mDevice;
		const MemoryManager*   mMemoryAllocator;
		const SwapChain*       mSwapChain;
		const ShaderManager*   mShaderManager;
	};
}