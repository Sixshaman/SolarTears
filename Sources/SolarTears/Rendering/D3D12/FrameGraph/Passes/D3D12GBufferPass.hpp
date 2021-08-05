#pragma once

#include "../D3D12RenderPass.hpp"
#include "../../../Common/FrameGraph/Passes/GBufferPass.hpp"
#include <dxc/dxcapi.h>

namespace D3D12
{
	class FrameGraphBuilder;
	class DeviceFeatures;

	class GBufferPass: public RenderPass, public GBufferPassBase
	{
		constexpr static DXGI_FORMAT ColorOutputFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

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
		GBufferPass(ID3D12Device8* device, const FrameGraphBuilder* frameGraphBuilder, const std::string& passName, uint32_t frame);
		~GBufferPass();

		void RecordExecution(ID3D12GraphicsCommandList6* commandList, const RenderableScene* scene, const ShaderManager* shaderManager, const FrameGraphConfig& frameGraphConfig) const override;

		ID3D12PipelineState* FirstPipeline() const override;

		virtual UINT GetPassDescriptorCountNeeded()                                                                               override;
		virtual void ValidatePassDescriptors(D3D12_GPU_DESCRIPTOR_HANDLE prevHeapStart, D3D12_GPU_DESCRIPTOR_HANDLE newHeapStart) override;

		virtual void RequestSceneDescriptors(DescriptorCreator* sceneDescriptorCreator)        override;
		virtual void ValidateSceneDescriptors(const DescriptorCreator* sceneDescriptorCreator) override;

	public:
		static void OnAdd(FrameGraphBuilder* frameGraphBuilder, const std::string& passName);

	private:
		void LoadShaders(const ShaderManager* shaderManager, IDxcBlobEncoding** outStaticVertexShader, IDxcBlobEncoding** outRigidVertexShader, IDxcBlobEncoding** outPixelShader);
		void CreateRootSignature(const ShaderManager* shaderManager, ID3D12Device8* device, IDxcBlobEncoding* rigidVertexShader, IDxcBlobEncoding* pixelShader);
		void CreateGBufferPipelineState(ID3D12Device8* device, IDxcBlobEncoding* vertexShader, IDxcBlobEncoding* pixelShader, ID3D12PipelineState** outPipelineState);

	private:
		D3D12_CPU_DESCRIPTOR_HANDLE mColorsRenderTarget;

		D3D12_GPU_DESCRIPTOR_HANDLE mSceneStaticObjectsTable;
		D3D12_GPU_DESCRIPTOR_HANDLE mSceneRigidObjectsTable;
		D3D12_GPU_DESCRIPTOR_HANDLE mSceneMaterialsTable;
		D3D12_GPU_DESCRIPTOR_HANDLE mSceneTexturesTable;

		D3D12_VIEWPORT mViewport;
		D3D12_RECT     mScissorRect;

		wil::com_ptr_nothrow<ID3D12RootSignature> mRootSignature;
		wil::com_ptr_nothrow<ID3D12PipelineState> mStaticPipelineState;
		wil::com_ptr_nothrow<ID3D12PipelineState> mRigidPipelineState;
	};
}