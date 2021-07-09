#pragma once

#include "RenderableSceneBase.hpp"
#include <span>

class FrameCounter;

//Class for scene functions common to Vulkan and D3D12
class ModernRenderableScene: public RenderableSceneBase
{
	friend class ModernRenderableSceneBuilder;

	struct ObjectUpdateMetadata
	{
		RenderableSceneMeshHandle MeshHandle;
		uint32_t                  ObjectDataIndex;
	};

public:
	ModernRenderableScene(const FrameCounter* frameCounter, uint64_t constantDataAlignment);
	~ModernRenderableScene();

	void UpdateSceneObjects(const FrameDataUpdateInfo& frameUpdate, const std::span<ObjectDataUpdateInfo> objectUpdates) override final;

protected:
	uint64_t CalculatePerObjectDataOffset(uint32_t currentFrameResourceIndex) const;
	uint64_t CalculatePerFrameDataOffset(uint32_t currentFrameResourceIndex)  const;

protected:
	//Created from inside
	const FrameCounter* mFrameCounterRef;

protected:
	//Created from outside
	std::vector<ObjectUpdateMetadata> mPrevFrameMeshUpdates; //Sorted, ping-pongs with mNextFrameMeshUpdates
	std::vector<ObjectUpdateMetadata> mNextFrameMeshUpdates; //Sorted, ping-pongs with mPrevFrameMeshUpdates

	std::vector<RenderableSceneMeshHandle> mCurrFrameMeshUpdates; //Sorted, used immediately each frame for rendering

	std::vector<PerObjectData> mPrevFrameDataToUpdate;
	std::vector<PerObjectData> mCurrFrameDataToUpdate;

	void* mSceneConstantDataBufferPointer; //Constant buffer is persistently mapped

	uint32_t mGBufferObjectChunkDataSize;
	uint32_t mGBufferFrameChunkDataSize;
};