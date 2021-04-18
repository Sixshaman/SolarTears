#pragma once

#include "../D3D12RenderPass.hpp"

namespace D3D12
{
	class FrameGraphBuilder;
	class DeviceFeatures;

	class GBufferPass: public RenderPass
	{
		constexpr static DXGI_FORMAT ColorOutputFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

	public:
		static constexpr std::string_view ColorBufferImageId = "GBufferPass-ColorBufferImage";

	public:
		~GBufferPass();

		void RecordExecution(ID3D12GraphicsCommandList6* commandList, const RenderableScene* scene, const ShaderManager* shaderManager, const FrameGraphConfig& frameGraphConfig) const override;

		ID3D12PipelineState* FirstPipeline() const override;

		static void Register(FrameGraphBuilder* frameGraphBuilder, const std::string& passName);

	private:
		GBufferPass(ID3D12Device8* device, const FrameGraphBuilder* frameGraphBuilder, const std::string& passName);

	private:
		void CreatePipelineState(ID3D12Device8* device, const ShaderManager* shaderManager, const FrameGraphConfig* frameGraphConfig);

	private:
		D3D12_CPU_DESCRIPTOR_HANDLE mColorsRenderTarget;
		uint32_t                    mFrameGraphTextureIndex;

		wil::com_ptr_nothrow<ID3D12PipelineState> mPipelineState;
	};
}