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

	class FrameGraphBuilder final: public ModernFrameGraphBuilder
	{
	public:
		FrameGraphBuilder(FrameGraph* graphToBuild, const SwapChain* swapChain);
		~FrameGraphBuilder();

		ID3D12Device8* GetDevice() const;

		const ShaderManager* GetShaderManager() const;

		ID3D12Resource2*            GetRegisteredResource(uint32_t passIndex,          uint_fast16_t subresourceIndex, uint32_t frame) const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetRegisteredSubresourceSrvUav(uint32_t passIndex, uint_fast16_t subresourceIndex, uint32_t frame) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetRegisteredSubresourceRtv(uint32_t passIndex,    uint_fast16_t subresourceIndex, uint32_t frame) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetRegisteredSubresourceDsv(uint32_t passIndex,    uint_fast16_t subresourceIndex, uint32_t frame) const;

		DXGI_FORMAT           GetRegisteredSubresourceFormat(uint32_t passIndex, uint_fast16_t subresourceIndex) const;
		D3D12_RESOURCE_STATES GetRegisteredSubresourceState(uint32_t passIndex, uint_fast16_t subresourceIndex)  const;

		D3D12_RESOURCE_STATES GetPreviousPassSubresourceState(uint32_t passIndex, uint_fast16_t subresourceIndex) const;
		D3D12_RESOURCE_STATES GetNextPassSubresourceState(uint32_t passIndex, uint_fast16_t subresourceIndex)     const;

		D3D12_GPU_DESCRIPTOR_HANDLE GetFrameGraphSrvHeapStart() const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetFrameGraphRtvHeapStart() const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetFrameGraphDsvHeapStart() const;

		void Build(ID3D12Device8* device, const ShaderManager* shaderManager, const MemoryManager* memoryManager);

	private:
		D3D12_COMMAND_LIST_TYPE PassClassToListType(RenderPassClass passType);

	private:
		//Registers subresource api-specific metadata
		void InitMetadataPayloads() override final;

		//Checks if the usage of the subresource with subresourceInfoIndex includes reading
		bool IsReadSubresource(uint32_t subresourceInfoIndex) override final;

		//Checks if the usage of the subresource with subresourceInfoIndex includes writing
		bool IsWriteSubresource(uint32_t subresourceInfoIndex) override final;

		//Propagates API-specific subresource data
		void PropagateSubresourcePayloadData() override final;

		//Creates a new render pass
		void CreatePassObject(const FrameGraphDescription::RenderPassName& passName, RenderPassType passType, uint32_t frame) override final;

		//Gives a free render pass span id
		uint32_t NextPassSpanId() override final;

		//Creates image objects
		void CreateTextures() override final;

		//Creates image view objects
		void CreateTextureViews() override final;

		//Add a barrier to execute before a pass
		uint32_t AddBeforePassBarrier(uint32_t metadataIndex) override final;

		//Add a barrier to execute before a pass
		uint32_t AddAfterPassBarrier(uint32_t metadataIndex) override final;

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