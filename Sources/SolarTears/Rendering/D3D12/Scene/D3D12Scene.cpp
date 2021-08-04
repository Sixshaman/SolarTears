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

D3D12_GPU_VIRTUAL_ADDRESS D3D12::RenderableScene::GetCurrentFrameFrameData() const
{
	return mSceneDynamicConstantBuffer->GetGPUVirtualAddress() + GetBaseFrameDataOffset(mFrameCounterRef->GetFrameCount() % Utils::InFlightFrameCount);
}

D3D12_GPU_VIRTUAL_ADDRESS D3D12::RenderableScene::GetCurrentFrameObjectData() const
{
	return mSceneDynamicConstantBuffer->GetGPUVirtualAddress() + GetBaseRigidObjectDataOffset(mFrameCounterRef->GetFrameCount() % Utils::InFlightFrameCount);
}
