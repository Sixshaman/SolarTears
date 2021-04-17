#pragma once

#include "../D3D12RenderPass.hpp"

class FrameGraphConfig;

namespace D3D12
{
	class FrameGraphBuilder;

	class CopyImagePass: public RenderPass
	{
	public:
		static constexpr std::string_view SrcImageId = "CopyImagePass-SrcImage";
		static constexpr std::string_view DstImageId = "CopyImagePass-DstImage";

	public:
		static void Register(FrameGraphBuilder* frameGraphBuilder, const std::string& passName);
		~CopyImagePass();

		void RecordExecution(ID3D12GraphicsCommandList6* commandList, const RenderableScene* scene, const FrameGraphConfig& frameGraphConfig) const override;

		ID3D12PipelineState* FirstPipeline() const override;

	private:
		CopyImagePass(ID3D12Device8* device, const FrameGraphBuilder* frameGraphBuilder, const std::string& currentPassName);

	private:
		ID3D12Resource* mSrcImageRef;
		ID3D12Resource* mDstImageRef;
	};
}