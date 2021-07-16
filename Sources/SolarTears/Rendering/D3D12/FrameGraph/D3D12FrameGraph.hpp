#pragma once

#include <d3d12.h>
#include <vector>
#include "D3D12RenderPass.hpp"
#include "../../Common/FrameGraph/FrameGraphConfig.hpp"
#include "../../Common/FrameGraph/ModernFrameGraph.hpp"

class ThreadPool;

namespace D3D12
{
	class WorkerCommandLists;
	class RenderableScene;
	class SrvDescriptorManager;
	class SwapChain;
	class DeviceQueues;

	class FrameGraph: public ModernFrameGraph
	{
		friend class FrameGraphDescriptorCreator;
		friend class FrameGraphBuilder;

	public:
		FrameGraph(FrameGraphConfig&& frameGraphConfig);
		~FrameGraph();

		void Traverse(ThreadPool* threadPool, const WorkerCommandLists* commandLists, const RenderableScene* scene, const ShaderManager* shaderManager, const SrvDescriptorManager* descriptorManager, DeviceQueues* deviceQueues, uint32_t frameIndex, uint32_t swapchainImageIndex);

	private:
		void SwitchBarrierTextures(uint32_t swapchainImageIndex, uint32_t frameIndex);

		void BeginCommandList(ID3D12GraphicsCommandList* commandList, ID3D12CommandAllocator* commandAllocator, uint32_t dependencyLevelSpanIndex) const;
		void EndCommandList(ID3D12GraphicsCommandList* commandList)                                                                                const;

		void RecordGraphicsPasses(ID3D12GraphicsCommandList6* commandList, const RenderableScene* scene, const ShaderManager* shaderManager, uint32_t dependencyLevelSpanIndex, uint32_t frameIndex, uint32_t swapchainImageIndex) const;

	private:
		std::vector<ID3D12Resource2*> mTextures;

		std::vector<std::unique_ptr<RenderPass>> mRenderPasses; //All render passes (sorted by dependency level)

		std::vector<wil::com_ptr_nothrow<ID3D12Resource2>> mOwnedResources; //All resources that the frame graph owns
		wil::com_ptr_nothrow<ID3D12Heap>                   mTextureHeap;

		std::vector<D3D12_RESOURCE_BARRIER> mResourceBarriers;

		D3D12_GPU_DESCRIPTOR_HANDLE                mPassDescriptorsStart;
		wil::com_ptr_nothrow<ID3D12DescriptorHeap> mRtvDescriptorHeap;
		wil::com_ptr_nothrow<ID3D12DescriptorHeap> mDsvDescriptorHeap;

		//Used to track the command buffers that were used to record render passes
		std::vector<ID3D12CommandList*> mFrameRecordedGraphicsCommandLists;
	};
}