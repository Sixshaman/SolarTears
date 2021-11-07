#include "D3D12Scene.hpp"
#include "../D3D12Utils.hpp"
#include "../../Common/RenderingUtils.hpp"
#include "../../Common/Scene/RenderableSceneMisc.hpp"
#include "../D3D12Shaders.hpp"
#include "../D3D12WorkerCommandLists.hpp"
#include "../D3D12DeviceQueues.hpp"
#include <array>

D3D12::RenderableScene::RenderableScene(): ModernRenderableScene(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
{
	for(uint32_t frameIndex = 0; frameIndex < Utils::InFlightFrameCount; frameIndex++)
	{
		mUploadCopyFenceValues[frameIndex] = 0;
	}
}

D3D12::RenderableScene::~RenderableScene()
{
	if(mSceneUploadBuffer)
	{
		mSceneUploadBuffer->Unmap(0, nullptr);
	}
}

void D3D12::RenderableScene::CopyUploadedSceneObjects(WorkerCommandLists* commandLists, DeviceQueues* deviceQueues, uint32_t frameResourceIndex)
{
	ID3D12GraphicsCommandList* cmdList      = commandLists->GetMainThreadCopyCommandList();
	ID3D12CommandAllocator*    cmdAllocator = commandLists->GetMainThreadCopyCommandAllocator(frameResourceIndex);

	THROW_IF_FAILED(cmdList->Reset(cmdAllocator, nullptr));

	cmdList->CopyBufferRegion(mSceneConstantBuffer.get(), GetBaseFrameDataOffset(), mSceneUploadBuffer.get(), GetUploadFrameDataOffset(frameResourceIndex), mFrameChunkDataSize);
	for(uint32_t updateIndex = 0; updateIndex < mCurrFrameUpdatedObjectCount; updateIndex++)
	{
		uint32_t meshIndex = mCurrFrameRigidMeshUpdateIndices[updateIndex];

		uint64_t objectDataOffset       = GetObjectDataOffset(meshIndex);
		uint64_t objectUploadDataOffset = GetUploadRigidObjectDataOffset(frameResourceIndex, meshIndex) - GetStaticObjectCount();
		cmdList->CopyBufferRegion(mSceneConstantBuffer.get(), objectDataOffset, mSceneUploadBuffer.get(), objectUploadDataOffset, mObjectChunkDataSize);
	}

	THROW_IF_FAILED(cmdList->Close());

	std::array commandListHandles = {(ID3D12CommandList*)cmdList};
	deviceQueues->CopyQueueHandle()->ExecuteCommandLists((UINT)commandListHandles.size(), commandListHandles.data());

	mUploadCopyFenceValues[frameResourceIndex] = deviceQueues->CopyFenceSignal();
}

UINT64 D3D12::RenderableScene::GetUploadFenceValue(uint32_t frameResourceIndex) const
{
	assert(frameResourceIndex < Utils::InFlightFrameCount);
	return mUploadCopyFenceValues[frameResourceIndex];
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
