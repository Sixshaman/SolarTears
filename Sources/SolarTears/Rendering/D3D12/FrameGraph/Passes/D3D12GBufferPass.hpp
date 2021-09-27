#pragma once

#include "../D3D12RenderPass.hpp"
#include "../../../Common/FrameGraph/Passes/GBufferPass.hpp"
#include "../D3D12FrameGraphMisc.hpp"
#include <dxc/dxcapi.h>
#include <span>

namespace D3D12
{
	class FrameGraphBuilder;
	class DeviceFeatures;

	class GBufferPass: public RenderPass, public GBufferPassBase
	{
		enum class GBufferRootBindings: UINT
		{
			MaterialIndex = 0,
			ObjectIndex,
			PerObjectBuffers,
			Textures,
			Materials,
			PerFrameBuffer,

			Count
		};

	public:
		inline static void RegisterSubesources(std::span<SubresourceMetadataPayload> inoutMetadataPayloads);
		inline static bool PropagateSubresourceInfos(std::span<SubresourceMetadataPayload> inoutMetadataPayloads);

	public:
		GBufferPass(const FrameGraphBuilder* frameGraphBuilder, uint32_t passIndex, uint32_t frame);
		~GBufferPass();

		void RecordExecution(ID3D12GraphicsCommandList6* commandList, const RenderableScene* scene, const FrameGraphConfig& frameGraphConfig, uint32_t frameResourceIndex) const override;

		ID3D12PipelineState* FirstPipeline() const override;

		virtual UINT GetPassDescriptorCountNeeded()                                                                               override;
		virtual void ValidatePassDescriptors(D3D12_GPU_DESCRIPTOR_HANDLE prevHeapStart, D3D12_GPU_DESCRIPTOR_HANDLE newHeapStart) override;

		virtual void RequestSceneDescriptors(DescriptorCreator* sceneDescriptorCreator)        override;
		virtual void ValidateSceneDescriptors(const DescriptorCreator* sceneDescriptorCreator) override;

	private:
		void LoadShaders(const ShaderManager* shaderManager, IDxcBlobEncoding** outStaticVertexShader, IDxcBlobEncoding** outRigidVertexShader, IDxcBlobEncoding** outPixelShader);
		void CreateRootSignature(const ShaderManager* shaderManager, ID3D12Device8* device, IDxcBlobEncoding* rigidVertexShader, IDxcBlobEncoding* pixelShader);
		void CreateGBufferPipelineState(ID3D12Device8* device, IDxcBlobEncoding* vertexShader, IDxcBlobEncoding* pixelShader, ID3D12PipelineState** outPipelineState);

	private:
		D3D12_CPU_DESCRIPTOR_HANDLE mColorsRenderTarget;

		D3D12_GPU_DESCRIPTOR_HANDLE mSceneTexturesTable;
		D3D12_GPU_DESCRIPTOR_HANDLE mSceneMaterialsTable;
		D3D12_GPU_DESCRIPTOR_HANDLE mSceneStaticObjectsTable;
		D3D12_GPU_DESCRIPTOR_HANDLE mSceneRigidObjectsTable[Utils::InFlightFrameCount];

		D3D12_VIEWPORT mViewport;
		D3D12_RECT     mScissorRect;

		wil::com_ptr_nothrow<ID3D12RootSignature> mRootSignature;
		wil::com_ptr_nothrow<ID3D12PipelineState> mStaticPipelineState;
		wil::com_ptr_nothrow<ID3D12PipelineState> mRigidPipelineState;
	};
}

#include "D3D12GBufferPass.inl"