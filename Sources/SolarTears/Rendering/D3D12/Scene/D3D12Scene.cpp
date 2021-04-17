#include "D3D12Scene.hpp"
#include "../D3D12Utils.hpp"
#include "../../../Core/Scene/SceneDescription.hpp"
#include "../../RenderableSceneMisc.hpp"
#include "../D3D12Shaders.hpp"
#include <array>

D3D12::RenderableScene::RenderableScene(const FrameCounter* frameCounter): RenderableSceneBase(D3D12Utils::InFlightFrameCount), mFrameCounterRef(frameCounter), 
                                                                           mSceneDataConstantObjectBufferOffset(0), mSceneDataConstantFrameBufferOffset(0),
	                                                                       mSceneConstantDataBufferPointer(nullptr)
{
	mScheduledSceneUpdates.push_back(RenderableSceneBase::ScheduledSceneUpdate());
	mObjectDataScheduledUpdateIndices.push_back(0);
	mScenePerObjectData.push_back(RenderableSceneBase::PerObjectData());

	mGBufferObjectChunkDataSize = sizeof(RenderableSceneBase::PerObjectData);
	mGBufferFrameChunkDataSize  = sizeof(RenderableSceneBase::PerFrameData);
}

D3D12::RenderableScene::~RenderableScene()
{
	mSceneConstantBuffer->Unmap(0, nullptr);
}

void D3D12::RenderableScene::FinalizeSceneUpdating()
{
	uint32_t frameResourceIndex = mFrameCounterRef->GetFrameCount() % D3D12Utils::InFlightFrameCount;

	uint32_t        lastUpdatedObjectIndex = (uint32_t)(-1);
	uint32_t        freeSpaceIndex         = 0; //We go through all scheduled updates and replace those that have DirtyFrameCount = 0
	SceneUpdateType lastUpdateType         = SceneUpdateType::UPDATE_UNDEFINED;
	for(uint32_t i = 0; i < mScheduledSceneUpdatesCount; i++)
	{
		//TODO: make it without switch statement
		UINT64 bufferOffset  = 0;
		UINT64 updateSize    = 0;
		UINT64 flushSize     = 0;
		void*  updatePointer = nullptr;

		switch(mScheduledSceneUpdates[i].UpdateType)
		{
		case SceneUpdateType::UPDATE_OBJECT:
		{
			bufferOffset  = CalculatePerObjectDataOffset(mScheduledSceneUpdates[i].ObjectIndex, frameResourceIndex);
			updateSize    = sizeof(RenderableSceneBase::PerObjectData);
			flushSize     = mGBufferObjectChunkDataSize;
			updatePointer = &mScenePerObjectData[mScheduledSceneUpdates[i].ObjectIndex];
			break;
		}
		case SceneUpdateType::UPDATE_COMMON:
		{
			bufferOffset  = CalculatePerFrameDataOffset(frameResourceIndex);
			updateSize    = sizeof(RenderableSceneBase::PerFrameData);
			flushSize     = mGBufferFrameChunkDataSize;
			updatePointer = &mScenePerFrameData;
			break;
		}
		default:
		{
			break;
		}
		}

		memcpy((std::byte*)mSceneConstantDataBufferPointer + bufferOffset, updatePointer, updateSize);

		lastUpdatedObjectIndex = mScheduledSceneUpdates[i].ObjectIndex;
		lastUpdateType         = mScheduledSceneUpdates[i].UpdateType;

		mScheduledSceneUpdates[i].DirtyFramesCount -= 1;

		if(mScheduledSceneUpdates[i].DirtyFramesCount != 0) //Shift all updates by one more element, thus erasing all that have dirty frames count == 0
		{
			//Shift by one
			mScheduledSceneUpdates[freeSpaceIndex] = mScheduledSceneUpdates[i];
			freeSpaceIndex += 1;
		}
	}

	mScheduledSceneUpdatesCount = freeSpaceIndex;
}

void D3D12::RenderableScene::DrawObjectsOntoGBuffer(ID3D12GraphicsCommandList* commandList, const ShaderManager* shaderManager) const
{
	std::array sceneVertexBuffers = {mSceneVertexBufferView};
	commandList->IASetVertexBuffers(0, (UINT)sceneVertexBuffers.size(), sceneVertexBuffers.data());

	commandList->IASetIndexBuffer(&mSceneIndexBufferView);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D12_GPU_VIRTUAL_ADDRESS constantBufferAddress = mSceneConstantBuffer->GetGPUVirtualAddress();

	uint32_t frameResourceIndex = mFrameCounterRef->GetFrameCount() % D3D12Utils::InFlightFrameCount;
	UINT64 PerFrameOffset = CalculatePerFrameDataOffset(frameResourceIndex);

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

UINT64 D3D12::RenderableScene::CalculatePerObjectDataOffset(uint32_t objectIndex, uint32_t currentFrameResourceIndex) const
{
	return mSceneDataConstantObjectBufferOffset + (mScenePerObjectData.size() * currentFrameResourceIndex + objectIndex) * mGBufferObjectChunkDataSize;
}

UINT64 D3D12::RenderableScene::CalculatePerFrameDataOffset(uint32_t currentFrameResourceIndex) const
{
	return mSceneDataConstantFrameBufferOffset + currentFrameResourceIndex * mGBufferFrameChunkDataSize;
}