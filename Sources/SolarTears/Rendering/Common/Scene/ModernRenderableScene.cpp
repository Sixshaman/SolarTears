#include "ModernRenderableScene.hpp"
#include "../RenderingUtils.hpp"
#include "../../../Core/FrameCounter.hpp"

ModernRenderableScene::ModernRenderableScene(const FrameCounter* frameCounter, uint64_t constantDataAlignment): RenderableSceneBase(Utils::InFlightFrameCount), mFrameCounterRef(frameCounter)
{
	mSceneConstantDataBufferPointer = nullptr;

	mGBufferObjectChunkDataSize = sizeof(RenderableSceneBase::PerObjectData);
	mGBufferFrameChunkDataSize  = sizeof(RenderableSceneBase::PerFrameData);

	mGBufferObjectChunkDataSize = (uint32_t)Utils::AlignMemory(mGBufferObjectChunkDataSize, constantDataAlignment);
	mGBufferFrameChunkDataSize  = (uint32_t)Utils::AlignMemory(mGBufferFrameChunkDataSize,  constantDataAlignment);
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
		RenderableSceneMeshHandle updatedObjectPrevFrame = mPrevFrameMeshUpdates[prevFrameUpdateIndex].MeshHandle;
		RenderableSceneMeshHandle updatedObjectToMerge   = objectUpdates[objectUpdateIndex].ObjectId;

		if(updatedObjectPrevFrame < updatedObjectToMerge)
		{
			uint32_t prevDataIndex = mPrevFrameMeshUpdates[objectUpdateIndex].ObjectDataIndex;

			//Schedule 1 update for the current frame
			mCurrFrameMeshUpdates[currFrameUpdateIndex]  = updatedObjectPrevFrame;
			mCurrFrameDataToUpdate[currFrameUpdateIndex] = mPrevFrameDataToUpdate[prevDataIndex];

			//The remaining updates of the same object go to the next frame
			while(mPrevFrameMeshUpdates[++prevFrameUpdateIndex].MeshHandle == updatedObjectPrevFrame)
			{
				mNextFrameMeshUpdates[nextFrameUpdateIndex++] =
				{
					.MeshHandle      = updatedObjectPrevFrame,
					.ObjectDataIndex = currFrameUpdateIndex
				};
			}

			currFrameUpdateIndex++;
		}
		else if(updatedObjectPrevFrame == updatedObjectToMerge)
		{
			//Grab 1 update from objectUpdate into the current frame
			mCurrFrameMeshUpdates[currFrameUpdateIndex]  = updatedObjectToMerge;
			mCurrFrameDataToUpdate[currFrameUpdateIndex] = PackObjectData(objectUpdates[objectUpdateIndex].ObjectLocation);

			//Grab the same update for next (InFlightFrameCount - 1) frames
			for(int i = 1; i < Utils::InFlightFrameCount; i++)
			{
				mNextFrameMeshUpdates[nextFrameUpdateIndex++] = 
				{
					.MeshHandle      = updatedObjectToMerge,
					.ObjectDataIndex = currFrameUpdateIndex
				};
			}

			//The previous updates don't matter anymore, they get rewritten
			while(mPrevFrameMeshUpdates[++prevFrameUpdateIndex].MeshHandle == updatedObjectPrevFrame);

			//Go to the next object update
			objectUpdateIndex++;
			currFrameUpdateIndex++;
		}
		else if(updatedObjectPrevFrame > updatedObjectToMerge)
		{
			//Grab 1 update from objectUpdate into the current frame
			mCurrFrameMeshUpdates[currFrameUpdateIndex]  = updatedObjectToMerge;
			mCurrFrameDataToUpdate[currFrameUpdateIndex] = PackObjectData(objectUpdates[objectUpdateIndex].ObjectLocation);

			//Grab the same update for next (InFlightFrameCount - 1) frames
			for(int i = 1; i < Utils::InFlightFrameCount; i++)
			{
				mNextFrameMeshUpdates[nextFrameUpdateIndex++] = 
				{
					.MeshHandle      = updatedObjectToMerge,
					.ObjectDataIndex = currFrameUpdateIndex
				};
			}

			//Go to the next object update
			objectUpdateIndex++;
			currFrameUpdateIndex++;
		}
	}
	
	mNextFrameMeshUpdates[nextFrameUpdateIndex] = {.MeshHandle = INVALID_SCENE_MESH_HANDLE, .ObjectDataIndex = (uint32_t)(-1)};
	std::swap(mPrevFrameMeshUpdates, mNextFrameMeshUpdates);


	//Do not need to worry about calling vkFlushMemoryMappedRanges, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT will handle this
	uint32_t frameResourceIndex = mFrameCounterRef->GetFrameCount() % Utils::InFlightFrameCount;

	PerFrameData perFrameData = PackFrameData(frameUpdate.ViewMatrix, frameUpdate.ProjMatrix);

	uint64_t frameDataOffset = CalculatePerFrameDataOffset(frameResourceIndex);
	memcpy((std::byte*)mSceneConstantDataBufferPointer + frameDataOffset, &perFrameData, sizeof(PerObjectData));
	
	uint64_t objectDataOffset = CalculatePerObjectDataOffset(frameResourceIndex);
	for(auto updateIndex = 0; updateIndex < currFrameUpdateIndex; updateIndex++)
	{
		uint32_t meshId = mCurrFrameMeshUpdates[updateIndex].Id;

		uint64_t currentObjectDataOffset = objectDataOffset + mGBufferObjectChunkDataSize * (size_t)meshId;
		memcpy((std::byte*)mSceneConstantDataBufferPointer + currentObjectDataOffset, &mCurrFrameDataToUpdate[updateIndex], sizeof(PerObjectData));
	}

	std::swap(mPrevFrameDataToUpdate, mCurrFrameDataToUpdate);
}


uint64_t ModernRenderableScene::CalculatePerObjectDataOffset(uint32_t currentFrameResourceIndex) const
{
	//For each frame the object data starts immediately after the frame data
	return CalculatePerFrameDataOffset(currentFrameResourceIndex) + mGBufferFrameChunkDataSize;
}

uint64_t ModernRenderableScene::CalculatePerFrameDataOffset(uint32_t currentFrameResourceIndex) const
{
	uint64_t wholeFrameResourceSize = (mStaticSceneObjects.size() + mRigidSceneObjects.size()) * mGBufferObjectChunkDataSize + mGBufferFrameChunkDataSize;
	return currentFrameResourceIndex * wholeFrameResourceSize;
}