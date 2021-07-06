#include "OGLScene.hpp"
#include "../OGLFunctions.hpp"
#include "../OGLUtils.hpp"

OpenGL::RenderableScene::RenderableScene(): ClassicRenderableScene()
{
}

void OpenGL::RenderableScene::FinalizeSceneUpdating()
{
	//glMapBuffer(mSceneUniformBuffer, )

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

void OpenGL::RenderableScene::DrawObjectsOntoGBuffer() const
{
	glBindVertexArray(mSceneVertexArray);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mSceneElementBuffer);

	for(size_t meshIndex = 0; meshIndex < mSceneMeshes.size(); meshIndex++)
	{
		for(uint32_t subobjectIndex = mSceneMeshes[meshIndex].FirstSubobjectIndex; subobjectIndex < mSceneMeshes[meshIndex].AfterLastSubobjectIndex; subobjectIndex++)
		{
			glDrawElementsBaseVertex(GL_TRIANGLES, mSceneSubobjects[subobjectIndex].IndexCount, OGLUtils::FormatForIndexType<RenderableSceneIndex>, VoidPtrOffset(mSceneSubobjects[subobjectIndex].FirstIndex), mSceneSubobjects[subobjectIndex].VertexOffset);
		}
	}

	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}