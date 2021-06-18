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

	class FrameGraphBuilder: public ModernFrameGraphBuilder
	{
		struct SubresourceInfo
		{
			DXGI_FORMAT           Format;
			D3D12_RESOURCE_STATES State;
			bool                  BarrierPromotedFromCommon;
		};

	public:
		FrameGraphBuilder(ID3D12Device8* device, FrameGraph* graphToBuild, const RenderableScene* scene, const DeviceFeatures* deviceFeatures, const SwapChain* swapChain, const ShaderManager* shaderManager, const MemoryManager* memoryManager);
		~FrameGraphBuilder();

		void RegisterRenderPass(const std::string_view passName, RenderPassCreateFunc createFunc, RenderPassType passType);

		void SetPassSubresourceFormat(const std::string_view passName, const std::string_view subresourceId, DXGI_FORMAT format);
		void SetPassSubresourceState(const std::string_view passName,  const std::string_view subresourceId, D3D12_RESOURCE_STATES state);

		ID3D12Device8*          GetDevice()         const;
		const RenderableScene*  GetScene()          const;
		const SwapChain*        GetSwapChain()      const;
		const DeviceFeatures*   GetDeviceFeatures() const;
		const ShaderManager*    GetShaderManager()  const;
		const MemoryManager*    GetMemoryManager()  const;

		ID3D12Resource2*            GetRegisteredResource(const std::string_view passName,          const std::string_view subresourceId, uint32_t frame) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetRegisteredSubresourceSrvUav(const std::string_view passName, const std::string_view subresourceId, uint32_t frame) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetRegisteredSubresourceRtv(const std::string_view passName,    const std::string_view subresourceId, uint32_t frame) const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetRegisteredSubresourceDsv(const std::string_view passName,    const std::string_view subresourceId, uint32_t frame) const;

		DXGI_FORMAT           GetRegisteredSubresourceFormat(const std::string_view passName, const std::string_view subresourceId) const;
		D3D12_RESOURCE_STATES GetRegisteredSubresourceState(const std::string_view passName,  const std::string_view subresourceId) const;

		D3D12_RESOURCE_STATES GetPreviousPassSubresourceState(const std::string_view passName, const std::string_view subresourceId) const;
		D3D12_RESOURCE_STATES GetNextPassSubresourceState(const std::string_view passName,     const std::string_view subresourceId) const;

		D3D12_CPU_DESCRIPTOR_HANDLE GetFrameGraphSrvHeapStart() const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetFrameGraphRtvHeapStart() const;
		D3D12_CPU_DESCRIPTOR_HANDLE GetFrameGraphDsvHeapStart() const;

	private:
		//Creates a new subresource info record
		uint32_t AddSubresourceMetadata() override;

		//Creates a new subresource info record for present pass
		uint32_t AddPresentSubresourceMetadata() override;

		//Creates a new render pass
		void AddRenderPass(const RenderPassName& passName, uint32_t frame) override;

		//Gives a free render pass span id
		uint32_t NextPassSpanId() override;

		//Propagates subresource info (format, access flags, etc.) to a node from the previous one. Also initializes view key. Returns true if propagation succeeded or wasn't needed
		bool ValidateSubresourceViewParameters(SubresourceMetadataNode* node) override;

		//Allocates the storage for image views defined by sort keys
		void AllocateImageViews(const std::vector<uint64_t>& sortKeys, uint32_t frameCount, std::vector<uint32_t>& outViewIds) override;

		//Creates image objects
		void CreateTextures(const std::vector<TextureResourceCreateInfo>& textureCreateInfos, const std::vector<TextureResourceCreateInfo>& backbufferCreateInfos, uint32_t totalTextureCount) const override;

		//Creates image view objects
		void CreateTextureViews(const std::vector<TextureSubresourceCreateInfo>& textureViewCreateInfos) const override;

		//Add a barrier to execute before a pass
		uint32_t AddBeforePassBarrier(uint32_t imageIndex, RenderPassType prevPassType, uint32_t prevPassSubresourceInfoIndex, RenderPassType currPassType, uint32_t currPassSubresourceInfoIndex) override;

		//Add a barrier to execute before a pass
		uint32_t AddAfterPassBarrier(uint32_t imageIndex, RenderPassType currPassType, uint32_t currPassSubresourceInfoIndex, RenderPassType nextPassType, uint32_t nextPassSubresourceInfoIndex) override;

		//Adds a barrier to execute right after acquiring a swapchain image
		uint32_t AddAcquirePassBarrier(uint32_t imageIndex, RenderPassType nextPassType, uint32_t nextPassSubresourceInfoIndex) override;

		//Adds a barrier to execute right before presenting a swapchain image
		uint32_t AddPresentPassBarrier(uint32_t imageIndex, RenderPassType prevPassType, uint32_t prevPassSubresourceInfoIndex) override;

		//Initializes per-traverse command buffer info
		void InitializeTraverseData() const override;

		//Get the number of swapchain images
		uint32_t GetSwapchainImageCount() const override;

	private:
		FrameGraph* mD3d12GraphToBuild;

		std::unordered_map<RenderPassName, RenderPassCreateFunc> mRenderPassCreateFunctions;

		std::vector<SubresourceInfo> mSubresourceInfos;

		UINT mSrvUavCbvDescriptorCount;
		UINT mRtvDescriptorCount;
		UINT mDsvDescriptorCount;

		UINT mSrvUavCbvDescriptorSize;
		UINT mRtvDescriptorSize;
		UINT mDsvDescriptorSize;

		//Several things that might be needed to create some of the passes
		ID3D12Device8*         mDevice;
		const RenderableScene* mScene;
		const DeviceFeatures*  mDeviceFeatures;
		const MemoryManager*   mMemoryAllocator;
		const SwapChain*       mSwapChain;
		const ShaderManager*   mShaderManager;
	};
}