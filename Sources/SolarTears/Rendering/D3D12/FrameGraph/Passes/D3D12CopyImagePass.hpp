#pragma once

#include "../D3D12RenderPass.hpp"
#include "../../../Common/FrameGraph/Passes/CopyImagePass.hpp"

class FrameGraphConfig;

namespace D3D12
{
	class FrameGraphBuilder;

	class CopyImagePass: public RenderPass, public CopyImagePassBase
	{
	public:
		CopyImagePass(ID3D12Device8* device, const FrameGraphBuilder* frameGraphBuilder, const std::string& currentPassName, uint32_t frame);
		~CopyImagePass();

		ID3D12PipelineState* FirstPipeline() const override;

		void RecordExecution(ID3D12GraphicsCommandList6* commandList, const RenderableScene* scene, const ShaderManager* shaderManager, const FrameGraphConfig& frameGraphConfig) const override;

		void RevalidateSrvUavDescriptors(D3D12_GPU_DESCRIPTOR_HANDLE prevHeapStart, D3D12_GPU_DESCRIPTOR_HANDLE newHeapStart) override;

	public:
		static void OnAdd(FrameGraphBuilder* frameGraphBuilder, const std::string& passName);

	private:
		ID3D12Resource* mSrcImageRef;
		ID3D12Resource* mDstImageRef;
	};
}