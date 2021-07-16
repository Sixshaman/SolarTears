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

void D3D12::RenderableScene::PrepareDrawBuffers(ID3D12GraphicsCommandList* cmdList) const
{
	std::array sceneVertexBuffers = {mSceneVertexBufferView};
	cmdList->IASetVertexBuffers(0, (UINT)sceneVertexBuffers.size(), sceneVertexBuffers.data());

	cmdList->IASetIndexBuffer(&mSceneIndexBufferView);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void D3D12::RenderableScene::DrawStaticObjectsWithRootConstants(ID3D12GraphicsCommandList* cmdList, UINT materialIndexRootBinding) const
{
	for(size_t meshIndex = 0; meshIndex < mStaticSceneObjects.size(); meshIndex++)
	{
		for(uint32_t subobjectIndex = mStaticSceneObjects[meshIndex].FirstSubobjectIndex; subobjectIndex < mStaticSceneObjects[meshIndex].AfterLastSubobjectIndex; subobjectIndex++)
		{
			uint32_t materialIndex = mSceneSubobjects[subobjectIndex].MaterialIndex;
			cmdList->SetGraphicsRoot32BitConstant(materialIndexRootBinding, materialIndex, 0);

			cmdList->DrawIndexedInstanced(mSceneSubobjects[subobjectIndex].IndexCount, 1, mSceneSubobjects[subobjectIndex].FirstIndex, mSceneSubobjects[subobjectIndex].VertexOffset, 0);
		}
	}
}

void D3D12::RenderableScene::DrawRigidObjectsWithRootConstants(ID3D12GraphicsCommandList* cmdList, UINT objectIndexRootBinding, UINT materialIndexRootBinding) const
{
	for(size_t meshIndex = 0; meshIndex < mRigidSceneObjects.size(); meshIndex++)
	{
		cmdList->SetGraphicsRoot32BitConstant(objectIndexRootBinding, meshIndex, 0);

		for(uint32_t subobjectIndex = mRigidSceneObjects[meshIndex].FirstSubobjectIndex; subobjectIndex < mRigidSceneObjects[meshIndex].AfterLastSubobjectIndex; subobjectIndex++)
		{
			uint32_t materialIndex = mSceneSubobjects[subobjectIndex].MaterialIndex;
			cmdList->SetGraphicsRoot32BitConstant(materialIndexRootBinding, materialIndex, 0);

			cmdList->DrawIndexedInstanced(mSceneSubobjects[subobjectIndex].IndexCount, 1, mSceneSubobjects[subobjectIndex].FirstIndex, mSceneSubobjects[subobjectIndex].VertexOffset, 0);
		}
	}
}