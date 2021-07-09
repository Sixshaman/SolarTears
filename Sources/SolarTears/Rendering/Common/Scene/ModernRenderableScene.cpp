#include "ModernRenderableScene.hpp"
#include "../RenderingUtils.hpp"
#include "../../../Core/FrameCounter.hpp"

ModernRenderableScene::ModernRenderableScene(const FrameCounter* frameCounter): RenderableSceneBase(Utils::InFlightFrameCount), mFrameCounterRef(frameCounter)
{
	mCBufferAlignmentSize = 0;
	
	mSceneConstantDataBufferPointer = nullptr;

	mGBufferFrameChunkDataSize  = 0;
	mGBufferObjectChunkDataSize = 0;

	mScheduledMeshUpdates.push_back(INVALID_SCENE_MESH_HANDLE);
}

ModernRenderableScene::~ModernRenderableScene()
{
}

void ModernRenderableScene::UpdateSceneObjects(const FrameDataUpdateInfo& frameUpdate, const std::span<ObjectDataUpdateInfo> objectUpdates)
{
	uint32_t prevFrameUpdateIndex = 0;
	uint32_t currFrameUpdateIndex = 0;
	uint32_t nextFrameUpdateIndex = 0;

	uint32_t objectUpdateIndex = 0;

	//Merge mPrevFrameMeshUpdates and objectUpdates into updates for the next frame and leftovers, keeping the result sorted
	while(mPrevFrameMeshUpdates[prevFrameUpdateIndex].MeshHandle != INVALID_SCENE_MESH_HANDLE || objectUpdateIndex < objectUpdates.size())
	{
		RenderableSceneMeshHandle prevFrameUpdateId = mPrevFrameMeshUpdates[prevFrameUpdateIndex].MeshHandle;
		RenderableSceneMeshHandle objectUpdateId    = objectUpdates[objectUpdateIndex].ObjectId;

		if(prevFrameUpdateId < objectUpdateId)
		{
			uint32_t prevDataIndex = mPrevFrameMeshUpdates[objectUpdateIndex].ObjectDataIndex;

			//Schedule 1 update for the current frame
			mCurrFrameMeshUpdates[currFrameUpdateIndex]    = prevFrameUpdateId;
			mCurrFrameDataToUpdate[currFrameUpdateIndex++] = mPrevFrameDataToUpdate[prevDataIndex];

			//The remaining updates of the same object go to the next frame
			while(mPrevFrameMeshUpdates[++prevFrameUpdateIndex] == prevFrameUpdateId)
			{
				mNextFrameMeshUpdates[nextFrameUpdateIndex++] = prevFrameUpdateId;
			}
		}
		else if(prevFrameUpdateId == objectUpdateId)
		{
			//Grab 1 update from objectUpdate into the current frame
			mCurrFrameMeshUpdates[currFrameUpdateIndex]    = objectUpdateId;
			mCurrFrameDataToUpdate[currFrameUpdateIndex++] = PackObjectData(objectUpdates[objectUpdateIndex].ObjectLocation);

			//Grab the same update for next (InFlightFrameCount - 1) frames
			for(int i = 1; i < Utils::InFlightFrameCount; i++)
			{
				mNextFrameMeshUpdates[nextFrameUpdateIndex++] = objectUpdateId;
			}

			//The previous updates don't matter anymore, they get rewritten
			while(mPrevFrameMeshUpdates[++prevFrameUpdateIndex] == prevFrameUpdateId);

			//Go to the next object update
			objectUpdateIndex += 1;
		}
		else if(prevFrameUpdateId > objectUpdateId)
		{
			//Grab 1 update from objectUpdate into the current frame
			mCurrFrameMeshUpdates[currFrameUpdateIndex++] = objectUpdateId;

			//Grab the same update for next (InFlightFrameCount - 1) frames
			for(int i = 1; i < Utils::InFlightFrameCount; i++)
			{
				mNextFrameMeshUpdates[nextFrameUpdateIndex++] = objectUpdateId;
			}

			//Go to the next object update
			objectUpdateIndex += 1;
		}
	}
	
	mNextFrameMeshUpdates[nextFrameUpdateIndex] = {.MeshHandle = INVALID_SCENE_MESH_HANDLE, .ObjectDataIndex = (uint32_t)(-1)};
	std::swap(mPrevFrameMeshUpdates, mNextFrameMeshUpdates);


	//Do not need to worry about calling vkFlushMemoryMappedRanges, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT will handle this
	uint32_t frameResourceIndex = mFrameCounterRef->GetFrameCount() % Utils::InFlightFrameCount;

	PerFrameData perFrameData = PackFrameData(frameUpdate.ViewMatrix, frameUpdate.ProjMatrix);

	uint64_t frameDataOffset = CalculatePerFrameDataOffset(frameResourceIndex);
	memcpy((std::byte*)mSceneConstantDataBufferPointer + frameDataOffset, &perFrameData, sizeof(PerObjectData));
	
	for(auto updateIndex = 0; updateIndex < currFrameUpdateIndex; updateIndex++)
	{
		uint32_t meshId = mCurrFrameMeshUpdates[currFrameUpdateIndex++].Id;

		uint64_t objectDataOffset  = CalculatePerObjectDataOffset(meshId, frameResourceIndex);
		memcpy((std::byte*)mSceneConstantDataBufferPointer + objectDataOffset, &mCurrFrameDataToUpdate[updateIndex], sizeof(PerObjectData));
	}
}

void ModernRenderableScene::Init()
{
	mGBufferObjectChunkDataSize = sizeof(RenderableSceneBase::PerObjectData);
	mGBufferFrameChunkDataSize  = sizeof(RenderableSceneBase::PerFrameData);

	mGBufferObjectChunkDataSize = (uint32_t)Utils::AlignMemory(mGBufferObjectChunkDataSize, mCBufferAlignmentSize);
	mGBufferFrameChunkDataSize  = (uint32_t)Utils::AlignMemory(mGBufferFrameChunkDataSize,  mCBufferAlignmentSize);
}

uint64_t ModernRenderableScene::CalculatePerObjectDataOffset(uint32_t objectIndex, uint32_t currentFrameResourceIndex) const
{
	return mSceneDataConstantObjectBufferOffset + (mScenePerObjectData.size() * currentFrameResourceIndex + objectIndex) * mGBufferObjectChunkDataSize;
}

uint64_t ModernRenderableScene::CalculatePerFrameDataOffset(uint32_t currentFrameResourceIndex) const
{
	return mSceneDataConstantFrameBufferOffset + currentFrameResourceIndex * mGBufferFrameChunkDataSize;
}