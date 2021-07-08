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
	auto prevFrameUpdatePointer = mPrevFrameMeshUpdates.begin();
	auto currFrameUpdatePointer = mCurrFrameMeshUpdates.begin();
	auto nextFrameUpdatePointer = mNextFrameMeshUpdates.begin();
	auto objectUpdatePointer    = objectUpdates.begin();
	while(*prevFrameUpdatePointer != INVALID_SCENE_MESH_HANDLE || objectUpdatePointer != objectUpdates.end())
	{
		RenderableSceneMeshHandle prevFrameUpdateId = *prevFrameUpdatePointer;
		RenderableSceneMeshHandle objectUpdateId    = objectUpdatePointer->ObjectId;

		if(prevFrameUpdateId < objectUpdateId)
		{
			//Schedule 1 update for the current frame, the rest updates of the same object go to the next frame
			*currFrameUpdatePointer = prevFrameUpdateId;
			++currFrameUpdatePointer;

			while(*(++prevFrameUpdatePointer) == prevFrameUpdateId)
			{
				*nextFrameUpdatePointer = prevFrameUpdateId;
				++nextFrameUpdatePointer;
			}
		}
		else if(prevFrameUpdateId == objectUpdateId)
		{
			//Previous updates of this object don't matter anymore, grab 1 update from objectUpdate to the current frame and (InFlightFrameCount - 1) for next frames
			*currFrameUpdatePointer = prevFrameUpdateId;
			++currFrameUpdatePointer;

			for(int i = 1; i < Utils::InFlightFrameCount; i++)
			{
				*nextFrameUpdatePointer = objectUpdateId;
				++nextFrameUpdatePointer;
			}

			while(*(++prevFrameUpdatePointer) == prevFrameUpdateId); //Skip all objects with this Id

			++objectUpdatePointer;
		}
		else if(prevFrameUpdateId > objectUpdateId)
		{
			//Grab 1 update from objectUpdate to the current frame and (InFlightFrameCount - 1) for next frames
			*currFrameUpdatePointer = objectUpdateId;
			++currFrameUpdatePointer;

			for(int i = 1; i < Utils::InFlightFrameCount; i++)
			{
				*nextFrameUpdatePointer = objectUpdateId;
				++nextFrameUpdatePointer;
			}

			++objectUpdatePointer;
		}
	}

	*nextFrameUpdatePointer = INVALID_SCENE_MESH_HANDLE;
	std::swap(mPrevFrameMeshUpdates, mNextFrameMeshUpdates);


	//Do not need to worry about calling vkFlushMemoryMappedRanges, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT will handle this
	uint32_t frameResourceIndex = mFrameCounterRef->GetFrameCount() % Utils::InFlightFrameCount;

	PerFrameData perFrameData = PackFrameData(frameUpdate.ViewMatrix, frameUpdate.ProjMatrix);

	uint64_t frameDataOffset = CalculatePerFrameDataOffset(frameResourceIndex);
	memcpy((std::byte*)mSceneConstantDataBufferPointer + frameDataOffset, &perFrameData, sizeof(PerObjectData));
	
	for(auto update = mCurrFrameMeshUpdates.begin(); update != nextFrameUpdatePointer; update++)
	{
		uint64_t objectDataOffset  = CalculatePerObjectDataOffset(update->Id, frameResourceIndex);
		memcpy((std::byte*)mSceneConstantDataBufferPointer + objectDataOffset, &mScenePerObjectData[update->Id], sizeof(PerObjectData));
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