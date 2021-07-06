#include "ClassicRenderableScene.hpp"
#include "../RenderingUtils.hpp"
#include "../../../Core/FrameCounter.hpp"

ClassicRenderableScene::ClassicRenderableScene(): RenderableSceneBase(1)
{
	mScheduledSceneUpdates.push_back(RenderableSceneBase::ScheduledSceneUpdate());
	mObjectDataScheduledUpdateIndices.push_back(0);
	mScenePerObjectData.push_back(RenderableSceneBase::PerObjectData());
}

ClassicRenderableScene::~ClassicRenderableScene()
{
}

void ClassicRenderableScene::FinalizeSceneUpdatingImpl()
{	
	//uint32_t        lastUpdatedObjectIndex = (uint32_t)(-1);
	//uint32_t        freeSpaceIndex         = 0; //We go through all scheduled updates and replace those that have DirtyFrameCount = 0
	//SceneUpdateType lastUpdateType         = SceneUpdateType::UPDATE_UNDEFINED;
	//for(uint32_t i = 0; i < mScheduledSceneUpdatesCount; i++)
	//{
	//	//TODO: make it without switch statement
	//	uint64_t bufferOffset  = 0;
	//	uint64_t updateSize    = 0;
	//	uint64_t flushSize     = 0;
	//	void*  updatePointer = nullptr;

	//	switch(mScheduledSceneUpdates[i].UpdateType)
	//	{
	//	case SceneUpdateType::UPDATE_OBJECT:
	//	{
	//		bufferOffset  = CalculatePerObjectDataOffset(mScheduledSceneUpdates[i].ObjectIndex, frameResourceIndex);
	//		updateSize    = sizeof(RenderableSceneBase::PerObjectData);
	//		flushSize     = mGBufferObjectChunkDataSize;
	//		updatePointer = &mScenePerObjectData[mScheduledSceneUpdates[i].ObjectIndex];
	//		break;
	//	}
	//	case SceneUpdateType::UPDATE_COMMON:
	//	{
	//		bufferOffset  = CalculatePerFrameDataOffset(frameResourceIndex);
	//		updateSize    = sizeof(RenderableSceneBase::PerFrameData);
	//		flushSize     = mGBufferFrameChunkDataSize;
	//		updatePointer = &mScenePerFrameData;
	//		break;
	//	}
	//	default:
	//	{
	//		break;
	//	}
	//	}

	//	memcpy((std::byte*)mSceneConstantDataBufferPointer + bufferOffset, updatePointer, updateSize);

	//	lastUpdatedObjectIndex = mScheduledSceneUpdates[i].ObjectIndex;
	//	lastUpdateType         = mScheduledSceneUpdates[i].UpdateType;

	//	mScheduledSceneUpdates[i].DirtyFramesCount -= 1;

	//	if(mScheduledSceneUpdates[i].DirtyFramesCount != 0) //Shift all updates by one more element, thus erasing all that have dirty frames count == 0
	//	{
	//		//Shift by one
	//		mScheduledSceneUpdates[freeSpaceIndex] = mScheduledSceneUpdates[i];
	//		freeSpaceIndex += 1;
	//	}
	//}

	//mScheduledSceneUpdatesCount = freeSpaceIndex;
}