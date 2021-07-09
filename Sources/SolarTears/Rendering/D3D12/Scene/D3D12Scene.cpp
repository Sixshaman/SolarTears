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

void D3D12::RenderableScene::DrawObjectsOntoGBuffer(ID3D12GraphicsCommandList* commandList, const ShaderManager* shaderManager) const
{
	uint32_t frameResourceIndex = mFrameCounterRef->GetFrameCount() % Utils::InFlightFrameCount;

	UINT64 PerFrameOffset  = CalculatePerFrameDataOffset(frameResourceIndex);
	UINT64 PerObjectOffset = CalculatePerObjectDataOffset(frameResourceIndex);

	D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress = mSceneConstantBuffer->GetGPUVirtualAddress();
	commandList->SetGraphicsRootConstantBufferView(shaderManager->GBufferPerFrameBufferBinding, CalculatePerFrameDataOffset())

	std::array sceneVertexBuffers = {mSceneVertexBufferView};
	commandList->IASetVertexBuffers(0, (UINT)sceneVertexBuffers.size(), sceneVertexBuffers.data());

	commandList->IASetIndexBuffer(&mSceneIndexBufferView);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->SetGraphicsRootConstantBufferView(shaderManager->GBufferPerFrameBufferBinding, constantBufferAddress + PerFrameOffset);
	for(size_t meshIndex = 0; meshIndex < mSceneMeshes.size(); meshIndex++)
	{
		UINT64 PerObjectOffset = CalculatePerObjectDataOffset((uint32_t)meshIndex, frameResourceIndex);

		for(uint32_t subobjectIndex = mSceneMeshes[meshIndex].FirstSubobjectIndex; subobjectIndex < mSceneMeshes[meshIndex].AfterLastSubobjectIndex; subobjectIndex++)
		{
			//Bind ROOT CONSTANTS for material index

			//TODO: bindless
			commandList->SetGraphicsRootConstantBufferView(shaderManager->GBufferPerObjectBufferBinding, constantBufferAddress + PerObjectOffset);
			commandList->SetGraphicsRootDescriptorTable(shaderManager->GBufferTextureBinding, mSceneTextureDescriptors[mSceneSubobjects[subobjectIndex].TextureDescriptorSetIndex]);

			commandList->DrawIndexedInstanced(mSceneSubobjects[subobjectIndex].IndexCount, 1, mSceneSubobjects[subobjectIndex].FirstIndex, mSceneSubobjects[subobjectIndex].VertexOffset, 0);
		}
	}
}