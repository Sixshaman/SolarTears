#include "ModernRenderableScene.hpp"
#include "../RenderingUtils.hpp"
#include "../../../Core/FrameCounter.hpp"

ModernRenderableScene::ModernRenderableScene(uint64_t constantDataAlignment)
{
	mSceneUploadDataBufferPointer = nullptr;

	mMaterialDataSize     = 0;
	mStaticObjectDataSize = 0;

	mObjectChunkDataSize   = (uint32_t)Utils::AlignMemory(sizeof(BaseRenderableScene::PerObjectData), constantDataAlignment);
	mFrameChunkDataSize    = (uint32_t)Utils::AlignMemory(sizeof(BaseRenderableScene::PerFrameData),  constantDataAlignment);
	mMaterialChunkDataSize = (uint32_t)Utils::AlignMemory(sizeof(RenderableSceneMaterial),            constantDataAlignment);
}

ModernRenderableScene::~ModernRenderableScene()
{
}

void ModernRenderableScene::UpdateFrameData(const FrameDataUpdateInfo& frameUpdate, uint64_t frameNumber)
{
	uint32_t frameResourceIndex = frameNumber % Utils::InFlightFrameCount;

	DirectX::XMMATRIX projMatrix = DirectX::XMLoadFloat4x4(&frameUpdate.ProjMatrix);
	PerFrameData perFrameData = PackFrameData(frameUpdate.CameraLocation, projMatrix);

	uint64_t frameDataOffset = GetUploadFrameDataOffset(frameResourceIndex);
	memcpy((std::byte*)mSceneUploadDataBufferPointer + frameDataOffset, &perFrameData, sizeof(PerFrameData));
}

void ModernRenderableScene::UpdateRigidSceneObjects(const std::span<ObjectDataUpdateInfo> rigidObjectUpdates, uint64_t frameNumber)
{
	uint32_t prevFrameUpdateIndex = 0;
	uint32_t currFrameUpdateIndex = 0;
	uint32_t nextFrameUpdateIndex = 0;

	uint32_t objectUpdateIndex = 0;

	//Merge mPrevFrameMeshUpdates and rigidObjectUpdates into updates for the next frame and leftovers, keeping the result sorted
	while(mPrevFrameRigidMeshUpdates[prevFrameUpdateIndex].MeshHandleIndex != (uint32_t)(-1) || objectUpdateIndex < rigidObjectUpdates.size())
	{
		assert(rigidObjectUpdates[objectUpdateIndex].ObjectId > GetStaticObjectCount());

		uint32_t updatedObjectPrevFrameIndex = mPrevFrameRigidMeshUpdates[prevFrameUpdateIndex].MeshHandleIndex;
		uint32_t updatedObjectToMergeIndex   = rigidObjectUpdates[objectUpdateIndex].ObjectId;

		if(updatedObjectPrevFrameIndex < updatedObjectToMergeIndex)
		{
			uint32_t prevDataIndex = mPrevFrameRigidMeshUpdates[objectUpdateIndex].ObjectDataIndex;

			//Schedule 1 update for the current frame
			mCurrFrameRigidMeshUpdateIndices[currFrameUpdateIndex] = updatedObjectPrevFrameIndex;
			mCurrFrameDataToUpdate[currFrameUpdateIndex]           = mPrevFrameDataToUpdate[prevDataIndex];

			//The remaining updates of the same object go to the next frame
			while(mPrevFrameRigidMeshUpdates[++prevFrameUpdateIndex].MeshHandleIndex == updatedObjectPrevFrameIndex)
			{
				mNextFrameRigidMeshUpdates[nextFrameUpdateIndex++] =
				{
					.MeshHandleIndex = updatedObjectPrevFrameIndex,
					.ObjectDataIndex = currFrameUpdateIndex
				};
			}

			currFrameUpdateIndex++;
		}
		else if(updatedObjectPrevFrameIndex == updatedObjectToMergeIndex)
		{
			//Grab 1 update from objectUpdate into the current frame
			mCurrFrameRigidMeshUpdateIndices[currFrameUpdateIndex] = updatedObjectToMergeIndex;
			mCurrFrameDataToUpdate[currFrameUpdateIndex]           = PackObjectData(rigidObjectUpdates[objectUpdateIndex++].NewObjectLocation);

			//Grab the same update for next (InFlightFrameCount - 1) frames
			for(int i = 1; i < Utils::InFlightFrameCount; i++)
			{
				mNextFrameRigidMeshUpdates[nextFrameUpdateIndex++] =
				{
					.MeshHandleIndex = updatedObjectToMergeIndex,
					.ObjectDataIndex = currFrameUpdateIndex
				};
			}

			//The previous updates don't matter anymore, they get rewritten
			while(mPrevFrameRigidMeshUpdates[++prevFrameUpdateIndex].MeshHandleIndex == updatedObjectPrevFrameIndex);

			currFrameUpdateIndex++;
		}
		else if(updatedObjectPrevFrameIndex > updatedObjectToMergeIndex)
		{
			//Grab 1 update from objectUpdate into the current frame
			mCurrFrameRigidMeshUpdateIndices[currFrameUpdateIndex] = updatedObjectToMergeIndex;
			mCurrFrameDataToUpdate[currFrameUpdateIndex]           = PackObjectData(rigidObjectUpdates[objectUpdateIndex++].NewObjectLocation);

			//Grab the same update for next (InFlightFrameCount - 1) frames
			for(int i = 1; i < Utils::InFlightFrameCount; i++)
			{
				mNextFrameRigidMeshUpdates[nextFrameUpdateIndex++] =
				{
					.MeshHandleIndex = updatedObjectToMergeIndex,
					.ObjectDataIndex = currFrameUpdateIndex
				};
			}

			currFrameUpdateIndex++;
		}
	}
	
	mNextFrameRigidMeshUpdates[nextFrameUpdateIndex] = {.MeshHandleIndex = (uint32_t)(-1), .ObjectDataIndex = (uint32_t)(-1)};
	std::swap(mPrevFrameRigidMeshUpdates, mNextFrameRigidMeshUpdates);

	mCurrFrameUpdatedObjectCount = currFrameUpdateIndex;

	uint32_t frameResourceIndex = frameNumber % Utils::InFlightFrameCount;
	for(uint32_t updateIndex = 0; updateIndex < mCurrFrameUpdatedObjectCount; updateIndex++)
	{
		uint32_t meshIndex = mCurrFrameRigidMeshUpdateIndices[updateIndex];

		uint64_t objectDataOffset = GetUploadRigidObjectDataOffset(frameResourceIndex, meshIndex) - GetStaticObjectCount();
		memcpy((std::byte*)mSceneUploadDataBufferPointer + objectDataOffset, &mCurrFrameDataToUpdate[updateIndex], sizeof(PerObjectData));
	}

	std::swap(mPrevFrameDataToUpdate, mCurrFrameDataToUpdate);
}

uint64_t ModernRenderableScene::GetMaterialDataOffset(uint32_t materialIndex) const
{
	return GetBaseMaterialDataOffset() + (uint64_t)materialIndex * mMaterialChunkDataSize;
}

uint64_t ModernRenderableScene::GetObjectDataOffset(uint32_t objectIndex) const
{
	//All object data starts at GetBaseStaticObjectDataOffset()
	return GetBaseStaticObjectDataOffset() + (uint64_t)objectIndex * mObjectChunkDataSize;
}

uint64_t ModernRenderableScene::GetBaseMaterialDataOffset() const
{
	//Material data is stored at the beginning of the static buffer
	return 0;
}

uint64_t ModernRenderableScene::GetBaseFrameDataOffset() const
{
	//Frame data is stored right after material data
	return GetBaseMaterialDataOffset() + mMaterialDataSize;
}

uint64_t ModernRenderableScene::GetBaseStaticObjectDataOffset() const
{
	//Object data is stored right after frame data
	return GetBaseFrameDataOffset() + mFrameDataSize;
}

uint64_t ModernRenderableScene::GetBaseRigidObjectDataOffset() const
{
	//Rigid object data is stored right after static object data
	return GetBaseStaticObjectDataOffset() + mStaticObjectDataSize;
}

uint64_t ModernRenderableScene::GetUploadFrameDataOffset(uint32_t frameResourceIndex) const
{
	//For each frame the frame data is stored in the beginning of this frame part of the dynamic buffer
	uint64_t wholeFrameResourceSize = (uint64_t)GetRigidObjectCount() * mObjectChunkDataSize + mFrameChunkDataSize;
	return frameResourceIndex * wholeFrameResourceSize;
}

uint64_t ModernRenderableScene::GetUploadRigidObjectDataOffset(uint32_t frameResourceIndex, uint32_t rigidObjectDataIndex) const
{
	//For each frame the object data starts immediately after the frame data
	return GetUploadFrameDataOffset(frameResourceIndex) + mFrameChunkDataSize + (uint64_t)rigidObjectDataIndex * mObjectChunkDataSize;
}

uint32_t ModernRenderableScene::GetMaterialCount() const
{
	return mMaterialDataSize / mMaterialChunkDataSize;
}

uint32_t ModernRenderableScene::GetStaticObjectCount() const
{
	return mStaticObjectDataSize / mObjectChunkDataSize;
}

uint32_t ModernRenderableScene::GetRigidObjectCount() const
{
	//Since mCurrFrameRigidMeshUpdates is allocated once and never shrinks, its size is equal to the total rigid object count
	return (uint32_t)mCurrFrameRigidMeshUpdateIndices.size();
}