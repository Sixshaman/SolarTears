#pragma once

#include "RenderableSceneBase.hpp"
#include <span>

class FrameCounter;

//Class for scene functions common to Vulkan and D3D12
class ModernRenderableScene: public RenderableSceneBase
{
	friend class ModernRenderableSceneBuilder;

public:
	ModernRenderableScene(const FrameCounter* frameCounter);
	~ModernRenderableScene();

	void UpdateSceneObjects(const FrameDataUpdateInfo& frameUpdate, const std::span<ObjectDataUpdateInfo> objectUpdates) override final;

protected:
	void Init();

	uint64_t CalculatePerObjectDataOffset(uint32_t objectIndex, uint32_t currentFrameResourceIndex) const;
	uint64_t CalculatePerFrameDataOffset(uint32_t currentFrameResourceIndex)                        const;

protected:
	//Created from inside
	const FrameCounter* mFrameCounterRef;

protected:
	//Created from outside
	std::vector<MeshSubobjectRange> mSceneMeshes;
	std::vector<SceneSubobject>     mSceneSubobjects;

	std::vector<RenderableSceneMeshHandle> mPrevFrameMeshUpdates; //Sorted, ping-pongs with mNextFrameMeshUpdates
	std::vector<RenderableSceneMeshHandle> mCurrFrameMeshUpdates; //Sorted, used immediately each frame for rendering
	std::vector<RenderableSceneMeshHandle> mNextFrameMeshUpdates; //Sorted, ping-pongs with mPrevFrameMeshUpdates

	void* mSceneConstantDataBufferPointer; //Constant buffer is persistently mapped

	uint32_t mGBufferObjectChunkDataSize;
	uint32_t mGBufferFrameChunkDataSize;

	uint64_t mSceneDataConstantObjectBufferOffset;
	uint64_t mSceneDataConstantFrameBufferOffset;

	uint64_t mCBufferAlignmentSize;
};