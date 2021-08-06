#include "ModernRenderableScene.hpp"
#include "../RenderingUtils.hpp"
#include "../../../Core/FrameCounter.hpp"

ModernRenderableScene::ModernRenderableScene(uint64_t constantDataAlignment)
{
	mSceneConstantDataBufferPointer = nullptr;

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

	uint64_t frameDataOffset = CalculateFrameDataOffset(frameResourceIndex);
	memcpy((std::byte*)mSceneConstantDataBufferPointer + frameDataOffset, &perFrameData, sizeof(PerFrameData));
}

void ModernRenderableScene::UpdateRigidSceneObjects(const std::span<ObjectDataUpdateInfo> rigidObjectUpdates, uint64_t frameNumber)
{
	uint32_t prevFrameUpdateIndex = 0;
	uint32_t currFrameUpdateIndex = 0;
	uint32_t nextFrameUpdateIndex = 0;

	uint32_t objectUpdateIndex = 0;

	//Merge mPrevFrameMeshUpdates and objectUpdates into updates for the next frame and leftovers, keeping the result sorted
	while(mPrevFrameRigidMeshUpdates[prevFrameUpdateIndex].MeshHandleIndex != (uint32_t)(-1) || objectUpdateIndex < rigidObjectUpdates.size())
	{
		assert(rigidObjectUpdates[objectUpdateIndex].ObjectId.ObjectType() == SceneObjectType::Rigid);

		uint32_t updatedObjectPrevFrameIndex = mPrevFrameRigidMeshUpdates[prevFrameUpdateIndex].MeshHandleIndex;
		uint32_t updatedObjectToMergeIndex   = rigidObjectUpdates[objectUpdateIndex].ObjectId.ObjectBufferIndex();

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

	uint32_t frameResourceIndex = frameNumber % Utils::InFlightFrameCount;

	for(auto updateIndex = 0; updateIndex < currFrameUpdateIndex; updateIndex++)
	{
		uint32_t meshIndex = mCurrFrameRigidMeshUpdateIndices[updateIndex];

		uint64_t objectDataOffset = CalculateRigidObjectDataOffset(frameResourceIndex, meshIndex);
		memcpy((std::byte*)mSceneConstantDataBufferPointer + objectDataOffset, &mCurrFrameDataToUpdate[updateIndex], sizeof(PerObjectData));
	}

	std::swap(mPrevFrameDataToUpdate, mCurrFrameDataToUpdate);
}

uint64_t ModernRenderableScene::CalculateRigidObjectDataOffset(uint32_t currentFrameResourceIndex, uint32_t objectIndex) const
{
	return GetBaseRigidObjectDataOffset(currentFrameResourceIndex) + (uint64_t)objectIndex * mObjectChunkDataSize;
}

uint64_t ModernRenderableScene::CalculateFrameDataOffset(uint32_t currentFrameResourceIndex) const
{
	return GetBaseFrameDataOffset(currentFrameResourceIndex);
}

uint64_t ModernRenderableScene::CalculateMaterialDataOffset(uint32_t materialIndex) const
{
	return GetBaseMaterialDataOffset() + (uint64_t)materialIndex * mMaterialChunkDataSize;
}

uint64_t ModernRenderableScene::CalculateStaticObjectDataOffset(uint32_t staticObjectIndex) const
{
	return GetBaseStaticObjectDataOffset() + (uint64_t)staticObjectIndex * mObjectChunkDataSize;
}

uint64_t ModernRenderableScene::GetBaseMaterialDataOffset() const
{
	//Materials are at the beginning of the static buffer
	return 0;
}

uint64_t ModernRenderableScene::GetBaseStaticObjectDataOffset() const
{
	//Static objects are right after materials in the static buffer
	return mMaterialDataSize;
}

uint64_t ModernRenderableScene::GetBaseRigidObjectDataOffset(uint32_t currentFrameResourceIndex) const
{
	//For each frame the object data starts immediately after the frame data
	return CalculateFrameDataOffset(currentFrameResourceIndex) + mFrameChunkDataSize;
}

uint64_t ModernRenderableScene::GetBaseFrameDataOffset(uint32_t currentFrameResourceIndex) const
{
	//For each frame the frame data is in the beginning of the dynamic buffer
	//Since mCurrFrameRigidMeshUpdates is allocated once and never shrinks, its size is equal to the total rigid object count
	uint64_t wholeFrameResourceSize = (uint64_t)mCurrFrameRigidMeshUpdateIndices.size() * mObjectChunkDataSize + mFrameChunkDataSize;
	return currentFrameResourceIndex * wholeFrameResourceSize;
}