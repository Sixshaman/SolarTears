#include "D3D12Scene.hpp"
#include "../D3D12Utils.hpp"
#include "../../Common/RenderingUtils.hpp"
#include "../../Common/Scene/RenderableSceneMisc.hpp"
#include "../D3D12Shaders.hpp"
#include <array>

D3D12::RenderableScene::RenderableScene(const FrameCounter* frameCounter): ModernRenderableScene(frameCounter, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
{
}

D3D12::RenderableScene::~RenderableScene()
{
	if(mSceneDynamicConstantBuffer)
	{
		mSceneDynamicConstantBuffer->Unmap(0, nullptr);
	}
}

void D3D12::RenderableScene::PrepareDrawBuffers(ID3D12GraphicsCommandList* cmdList) const
{
	std::array sceneVertexBuffers = {mSceneVertexBufferView};
	cmdList->IASetVertexBuffers(0, (UINT)sceneVertexBuffers.size(), sceneVertexBuffers.data());

	cmdList->IASetIndexBuffer(&mSceneIndexBufferView);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void D3D12::RenderableScene::DrawBakedPositionObjectsWithRootConstants(ID3D12GraphicsCommandList* cmdList, UINT materialIndexRootBinding) const
{
	for(uint32_t meshIndex = mStaticMeshSpan.Begin; meshIndex < mStaticMeshSpan.End; meshIndex++)
	{
		for(uint32_t submeshIndex = mSceneMeshes[meshIndex].FirstSubmeshIndex; submeshIndex < mSceneMeshes[meshIndex].AfterLastSubmeshIndex; submeshIndex++)
		{
			uint32_t materialIndex = mSceneSubmeshes[submeshIndex].MaterialIndex;
			cmdList->SetGraphicsRoot32BitConstant(materialIndexRootBinding, materialIndex, 0);

			cmdList->DrawIndexedInstanced(mSceneSubmeshes[submeshIndex].IndexCount, mSceneMeshes[meshIndex].InstanceCount, mSceneSubmeshes[submeshIndex].FirstIndex, mSceneSubmeshes[submeshIndex].VertexOffset, 0);
		}
	}
}

void D3D12::RenderableScene::DrawBufferedPositionObjectsWithRootConstants(ID3D12GraphicsCommandList* cmdList, UINT objectIndexRootBinding, UINT materialIndexRootBinding) const
{
	//The spans should follow each other
	assert(mStaticInstancedMeshSpan.End == mRigidMeshSpan.Begin);

	const uint32_t firstMeshIndex     = mStaticInstancedMeshSpan.Begin;
	const uint32_t afterLastMeshIndex = mRigidMeshSpan.End;

	uint32_t objectDataIndex = 0;
	for(uint32_t meshIndex = firstMeshIndex; meshIndex < afterLastMeshIndex; meshIndex++)
	{
		cmdList->SetGraphicsRoot32BitConstant(objectIndexRootBinding, objectDataIndex, 0);

		for(uint32_t submeshIndex = mSceneMeshes[meshIndex].FirstSubmeshIndex; submeshIndex < mSceneMeshes[meshIndex].AfterLastSubmeshIndex; submeshIndex++)
		{
			uint32_t materialIndex = mSceneSubmeshes[submeshIndex].MaterialIndex;
			cmdList->SetGraphicsRoot32BitConstant(materialIndexRootBinding, materialIndex, 0);

			cmdList->DrawIndexedInstanced(mSceneSubmeshes[submeshIndex].IndexCount, mSceneMeshes[meshIndex].InstanceCount, mSceneSubmeshes[submeshIndex].FirstIndex, mSceneSubmeshes[submeshIndex].VertexOffset, 0);
		}

		objectDataIndex += mSceneMeshes[meshIndex].InstanceCount;
	}
}