#include "D3D12Scene.hpp"
#include "../D3D12Utils.hpp"
#include "../../Common/RenderingUtils.hpp"
#include "../../Common/Scene/RenderableSceneMisc.hpp"
#include "../D3D12Shaders.hpp"
#include <array>

D3D12::RenderableScene::RenderableScene(): ModernRenderableScene(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
{
}

D3D12::RenderableScene::~RenderableScene()
{
	if(mSceneUploadBuffer)
	{
		mSceneUploadBuffer->Unmap(0, nullptr);
	}
}

void D3D12::RenderableScene::PrepareDrawBuffers(ID3D12GraphicsCommandList* cmdList) const
{
	std::array sceneVertexBuffers = {mSceneVertexBufferView};
	cmdList->IASetVertexBuffers(0, (UINT)sceneVertexBuffers.size(), sceneVertexBuffers.data());

	cmdList->IASetIndexBuffer(&mSceneIndexBufferView);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

D3D12_GPU_VIRTUAL_ADDRESS D3D12::RenderableScene::GetMaterialDataStart() const
{
	return mSceneConstantBuffer->GetGPUVirtualAddress() + GetBaseMaterialDataOffset();
}

D3D12_GPU_VIRTUAL_ADDRESS D3D12::RenderableScene::GetFrameDataStart() const
{
	return mSceneConstantBuffer->GetGPUVirtualAddress() + GetBaseFrameDataOffset();
}

D3D12_GPU_VIRTUAL_ADDRESS D3D12::RenderableScene::GetObjectDataStart() const
{
	return mSceneConstantBuffer->GetGPUVirtualAddress() + GetBaseStaticObjectDataOffset();
}

D3D12_GPU_VIRTUAL_ADDRESS D3D12::RenderableScene::GetMaterialDataStart(uint32_t materialIndex) const
{
	return mSceneConstantBuffer->GetGPUVirtualAddress() + GetMaterialDataOffset(materialIndex);
}

D3D12_GPU_VIRTUAL_ADDRESS D3D12::RenderableScene::GetObjectDataStart(uint32_t objectIndex) const
{
	return mSceneConstantBuffer->GetGPUVirtualAddress() + GetObjectDataOffset(objectIndex);
}
