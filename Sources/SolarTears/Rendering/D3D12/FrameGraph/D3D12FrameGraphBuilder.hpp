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
		constexpr static uint32_t TextureFlagBarrierCommonPromoted = 0x01; //The barrier is automatically promoted from COMMON state

		struct SubresourceInfo
		{
			DXGI_FORMAT           Format;
			D3D12_RESOURCE_STATES State;
		};

	public:
		FrameGraphBuilder(FrameGraph* graphToBuild, FrameGraphDescription&& frameGraphDescription, const SwapChain* swapChain);
		~FrameGraphBuilder();

		void SetPassSubresourceFormat(const std::string_view subresourceId, DXGI_FORMAT format);
		void SetPassSubresourceState(const std::string_view subresourceId, D3D12_RESOURCE_STATES state);

		ID3D12Device8* GetDevice() const;

		const ShaderManager* GetShaderManager() const;

		ID3D12Resource2*            GetRegisteredResource(const std::string_view passName,          const std::string_view subresourceId, uint32_t frame) const;
		D3D12_GPU_DESCRIPTOR_HANDLE GetRegisteredSubresourceSrvUav(const std::string_view passName, const std::string_view subresourceId, uint32_t frame) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetRegisteredSubresourceRtv(const std::string_view passName,    const std::string_view subresourceId, uint32_t frame) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetRegisteredSubresourceDsv(const std::string_view passName,    const std::string_view subresourceId, uint32_t frame) const;

		DXGI_FORMAT           GetRegisteredSubresourceFormat(const std::string_view subresourceId) const;
		D3D12_RESOURCE_STATES GetRegisteredSubresourceState(const std::string_view subresourceId)  const;

		D3D12_RESOURCE_STATES GetPreviousPassSubresourceState(const std::string_view subresourceId) const;
		D3D12_RESOURCE_STATES GetNextPassSubresourceState(const std::string_view subresourceId)     const;

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
		//Registers subresource ids for pass types
		void RegisterPassTypes(const std::span<RenderPassType>& passTypes) override final;

		//Checks if the usage of the subresource with subresourceInfoIndex includes reading
		bool IsReadSubresource(uint32_t subresourceInfoIndex) override final;

		//Checks if the usage of the subresource with subresourceInfoIndex includes writing
		bool IsWriteSubresource(uint32_t subresourceInfoIndex) override final;

		//Registers render pass related data in the graph (inputs, outputs, possibly shader bindings, etc)
		void RegisterPassInGraph(RenderPassType passType, const FrameGraphDescription::RenderPassName& passName) override final;

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

		std::unordered_map<RenderPassType, RenderPassAddFunc>    mPassAddFuncTable;
		std::unordered_map<RenderPassType, RenderPassCreateFunc> mPassCreateFuncTable;

		std::vector<SubresourceInfo> mSubresourceInfosFlat;

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