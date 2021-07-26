#include "ModernRenderableScene.hpp"
#include "../RenderingUtils.hpp"
#include "../../../Core/FrameCounter.hpp"

ModernRenderableScene::ModernRenderableScene(const FrameCounter* frameCounter, uint64_t constantDataAlignment): mFrameCounterRef(frameCounter)
{
	mSceneConstantDataBufferPointer = nullptr;

	mObjectChunkDataSize   = (uint32_t)Utils::AlignMemory(sizeof(RenderableSceneBase::PerObjectData), constantDataAlignment);
	mFrameChunkDataSize    = (uint32_t)Utils::AlignMemory(sizeof(RenderableSceneBase::PerFrameData),  constantDataAlignment);
	mMaterialChunkDataSize = (uint32_t)Utils::AlignMemory(sizeof(RenderableSceneBase::SceneMaterial), constantDataAlignment);
}

ModernRenderableScene::~ModernRenderableScene()
{
}

void ModernRenderableScene::UpdateRigidSceneObjects(const FrameDataUpdateInfo& frameUpdate, const std::span<ObjectDataUpdateInfo> rigidObjectUpdates)
{
	uint32_t prevFrameUpdateIndex = 0;
	uint32_t currFrameUpdateIndex = 0;
	uint32_t nextFrameUpdateIndex = 0;

	uint32_t objectUpdateIndex = 0;

	//Merge mPrevFrameMeshUpdates and objectUpdates into updates for the next frame and leftovers, keeping the result sorted
	while(mPrevFrameRigidMeshUpdates[prevFrameUpdateIndex].MeshIndex != (uint32_t)(-1) || objectUpdateIndex < rigidObjectUpdates.size())
	{
		assert(rigidObjectUpdates[objectUpdateIndex].ObjectId.ObjectType() == SceneObjectType::Rigid);

		uint32_t updatedObjectPrevFrameIndex = mPrevFrameRigidMeshUpdates[prevFrameUpdateIndex].MeshIndex;
		uint32_t updatedObjectToMergeIndex   = rigidObjectUpdates[objectUpdateIndex].ObjectId.ObjectBufferIndex();

		if(updatedObjectPrevFrameIndex < updatedObjectToMergeIndex)
		{
			uint32_t prevDataIndex = mPrevFrameRigidMeshUpdates[objectUpdateIndex].ObjectDataIndex;

			//Schedule 1 update for the current frame
			mCurrFrameRigidMeshUpdates[currFrameUpdateIndex] = updatedObjectPrevFrameIndex;
			mCurrFrameDataToUpdate[currFrameUpdateIndex]     = mPrevFrameDataToUpdate[prevDataIndex];

			//The remaining updates of the same object go to the next frame
			while(mPrevFrameRigidMeshUpdates[++prevFrameUpdateIndex].MeshIndex == updatedObjectPrevFrameIndex)
			{
				mNextFrameRigidMeshUpdates[nextFrameUpdateIndex++] =
				{
					.MeshIndex       = updatedObjectPrevFrameIndex,
					.ObjectDataIndex = currFrameUpdateIndex
				};
			}

			currFrameUpdateIndex++;
		}
		else if(updatedObjectPrevFrameIndex == updatedObjectToMergeIndex)
		{
			//Grab 1 update from objectUpdate into the current frame
			mCurrFrameRigidMeshUpdates[currFrameUpdateIndex] = updatedObjectToMergeIndex;
			mCurrFrameDataToUpdate[currFrameUpdateIndex]     = PackObjectData(rigidObjectUpdates[objectUpdateIndex++].NewObjectLocation);

			//Grab the same update for next (InFlightFrameCount - 1) frames
			for(int i = 1; i < Utils::InFlightFrameCount; i++)
			{
				mNextFrameRigidMeshUpdates[nextFrameUpdateIndex++] =
				{
					.MeshIndex       = updatedObjectToMergeIndex,
					.ObjectDataIndex = currFrameUpdateIndex
				};
			}

			//The previous updates don't matter anymore, they get rewritten
			while(mPrevFrameRigidMeshUpdates[++prevFrameUpdateIndex].MeshIndex == updatedObjectPrevFrameIndex);

			currFrameUpdateIndex++;
		}
		else if(updatedObjectPrevFrameIndex > updatedObjectToMergeIndex)
		{
			//Grab 1 update from objectUpdate into the current frame
			mCurrFrameRigidMeshUpdates[currFrameUpdateIndex] = updatedObjectToMergeIndex;
			mCurrFrameDataToUpdate[currFrameUpdateIndex]     = PackObjectData(rigidObjectUpdates[objectUpdateIndex++].NewObjectLocation);

			//Grab the same update for next (InFlightFrameCount - 1) frames
			for(int i = 1; i < Utils::InFlightFrameCount; i++)
			{
				mNextFrameRigidMeshUpdates[nextFrameUpdateIndex++] =
				{
					.MeshIndex       = updatedObjectToMergeIndex,
					.ObjectDataIndex = currFrameUpdateIndex
				};
			}

			currFrameUpdateIndex++;
		}
	}
	
	mNextFrameRigidMeshUpdates[nextFrameUpdateIndex] = {.MeshIndex = (uint32_t)(-1), .ObjectDataIndex = (uint32_t)(-1)};
	std::swap(mPrevFrameRigidMeshUpdates, mNextFrameRigidMeshUpdates);


	//Do not need to worry about calling vkFlushMemoryMappedRanges, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT will handle this
	uint32_t frameResourceIndex = mFrameCounterRef->GetFrameCount() % Utils::InFlightFrameCount;

	PerFrameData perFrameData = PackFrameData(frameUpdate.ViewMatrix, frameUpdate.ProjMatrix);

	uint64_t frameDataOffset = CalculateFrameDataOffset(frameResourceIndex);
	memcpy((std::byte*)mSceneConstantDataBufferPointer + frameDataOffset, &perFrameData, sizeof(PerObjectData));
	
	for(auto updateIndex = 0; updateIndex < currFrameUpdateIndex; updateIndex++)
	{
		uint32_t meshIndex = mCurrFrameRigidMeshUpdates[updateIndex];

		uint64_t objectDataOffset = CalculateRigidObjectDataOffset(frameResourceIndex, meshIndex);
		memcpy((std::byte*)mSceneConstantDataBufferPointer + objectDataOffset, &mCurrFrameDataToUpdate[updateIndex], sizeof(PerObjectData));
	}

	std::swap(mPrevFrameDataToUpdate, mCurrFrameDataToUpdate);
}

uint64_t ModernRenderableScene::CalculateRigidObjectDataOffset(uint32_t currentFrameResourceIndex, uint32_t objectIndex) const
{
	//For each frame the object data starts immediately after the frame data
	return CalculateFrameDataOffset(currentFrameResourceIndex) + mFrameChunkDataSize + (uint64_t)objectIndex * mObjectChunkDataSize;
}

uint64_t ModernRenderableScene::CalculateFrameDataOffset(uint32_t currentFrameResourceIndex) const
{
	//For each frame the frame data is in the beginning
	uint64_t wholeFrameResourceSize = (uint64_t)(mRigidMeshSpan.End - mRigidMeshSpan.Begin) * mObjectChunkDataSize + mFrameChunkDataSize;
	return currentFrameResourceIndex * wholeFrameResourceSize;
}

uint64_t ModernRenderableScene::CalculateMaterialDataOffset(uint32_t materialIndex) const
{
	//Materials are in the beginning of the buffer
	return (uint64_t)materialIndex * mMaterialChunkDataSize;
}