#include "D3D12Scene.hpp"
#include "../D3D12Utils.hpp"
#include "../../Common/RenderingUtils.hpp"
#include "../../../Core/Scene/SceneDescription.hpp"
#include "../../Common/Scene/RenderableSceneMisc.hpp"
#include "../D3D12Shaders.hpp"
#include <array>

D3D12::RenderableScene::RenderableScene(const FrameCounter* frameCounter): ModernRenderableScene(frameCounter, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
{
}

D3D12::RenderableScene::~RenderableScene()
{
	if(mSceneConstantBuffer)
	{
		mSceneConstantBuffer->Unmap(0, nullptr);
	}
}

void D3D12::RenderableScene::DrawObjectsOntoGBuffer(ID3D12GraphicsCommandList* commandList, const ShaderManager* shaderManager, ID3D12PipelineState* staticPipelineState, ID3D12PipelineState* rigidPipelineState) const
{
	uint32_t frameResourceIndex = mFrameCounterRef->GetFrameCount() % Utils::InFlightFrameCount;

	std::array sceneVertexBuffers = { mSceneVertexBufferView };
	commandList->IASetVertexBuffers(0, (UINT)sceneVertexBuffers.size(), sceneVertexBuffers.data());

	commandList->IASetIndexBuffer(&mSceneIndexBufferView);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->SetPipelineState(staticPipelineState);

	UINT64 PerFrameOffset  = CalculatePerFrameDataOffset(frameResourceIndex);
	UINT64 PerObjectOffset = CalculatePerObjectDataOffset(frameResourceIndex);

	D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress = mSceneConstantBuffer->GetGPUVirtualAddress();
	commandList->SetGraphicsRootConstantBufferView(shaderManager->GBufferPerFrameBufferBinding,  PerFrameOffset);
	commandList->SetGraphicsRootConstantBufferView(shaderManager->GBufferPerObjectBufferBinding, PerObjectOffset);

	commandList->SetGraphicsRootDescriptorTable(shaderManager->GBufferTextureBinding, mSceneTextureDescriptorsStart);
	for(size_t meshIndex = 0; meshIndex < mStaticSceneObjects.size(); meshIndex++)
	{
		for(uint32_t subobjectIndex = mStaticSceneObjects[meshIndex].FirstSubobjectIndex; subobjectIndex < mStaticSceneObjects[meshIndex].AfterLastSubobjectIndex; subobjectIndex++)
		{
			uint32_t materialIndex = mSceneSubobjects[subobjectIndex].MaterialIndex;
			commandList->SetGraphicsRoot32BitConstant(shaderManager->GBufferMaterialIndexBinding, materialIndex, 0);

			commandList->DrawIndexedInstanced(mSceneSubobjects[subobjectIndex].IndexCount, 1, mSceneSubobjects[subobjectIndex].FirstIndex, mSceneSubobjects[subobjectIndex].VertexOffset, 0);
		}
	}


	commandList->SetPipelineState(rigidPipelineState);
	for(size_t meshIndex = 0; meshIndex < mRigidSceneObjects.size(); meshIndex++)
	{
		commandList->SetGraphicsRoot32BitConstant(shaderManager->GBufferMaterialIndexBinding, meshIndex, 0);

		for(uint32_t subobjectIndex = mRigidSceneObjects[meshIndex].FirstSubobjectIndex; subobjectIndex < mRigidSceneObjects[meshIndex].AfterLastSubobjectIndex; subobjectIndex++)
		{
			uint32_t materialIndex = mSceneSubobjects[subobjectIndex].MaterialIndex;
			commandList->SetGraphicsRoot32BitConstant(shaderManager->GBufferMaterialIndexBinding, materialIndex, 0);

			commandList->DrawIndexedInstanced(mSceneSubobjects[subobjectIndex].IndexCount, 1, mSceneSubobjects[subobjectIndex].FirstIndex, mSceneSubobjects[subobjectIndex].VertexOffset, 0);
		}
	}
}