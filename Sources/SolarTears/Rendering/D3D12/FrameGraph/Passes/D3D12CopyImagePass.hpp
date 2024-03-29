#pragma once

#include "../D3D12RenderPass.hpp"
#include "../../../Common/FrameGraph/Passes/CopyImagePass.hpp"
#include "../D3D12FrameGraphMisc.hpp"
#include <span>

class FrameGraphConfig;

namespace D3D12
{
	class FrameGraphBuilder;

	class CopyImagePass: public RenderPass, public CopyImagePassBase
	{
	public:
		inline static void RegisterSubresources(std::span<SubresourceMetadataPayload> inoutMetadataPayloads);
		inline static bool PropagateSubresourceInfos(std::span<SubresourceMetadataPayload> inoutMetadataPayloads);

	public:
		CopyImagePass(const FrameGraphBuilder* frameGraphBuilder, uint32_t passIndex);
		~CopyImagePass();

		ID3D12PipelineState* FirstPipeline() const override;

		void RecordExecution(ID3D12GraphicsCommandList6* commandList, const RenderableScene* scene, const FrameGraphConfig& frameGraphConfig) const override;

		virtual UINT GetPassDescriptorCountNeeded()                                                                               override;
		virtual void ValidatePassDescriptors(D3D12_GPU_DESCRIPTOR_HANDLE prevHeapStart, D3D12_GPU_DESCRIPTOR_HANDLE newHeapStart) override;

		virtual void RequestSceneDescriptors(DescriptorCreator* sceneDescriptorCreator)        override;
		virtual void ValidateSceneDescriptors(const DescriptorCreator* sceneDescriptorCreator) override;

	private:
		ID3D12Resource* mSrcImageRef;
		ID3D12Resource* mDstImageRef;
	};
}

#include "D3D12CopyImagePass.inl"