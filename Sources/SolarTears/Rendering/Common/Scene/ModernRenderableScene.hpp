#pragma once

#include "RenderableSceneBase.hpp"
#include <span>

class FrameCounter;

//Class for scene functions common to Vulkan and D3D12
class ModernRenderableScene: public RenderableSceneBase
{
	friend class ModernRenderableSceneBuilder;

	struct RigidObjectUpdateMetadata
	{
		uint32_t MeshIndex;
		uint32_t ObjectDataIndex;
	};

public:
	ModernRenderableScene(const FrameCounter* frameCounter, uint64_t constantDataAlignment);
	~ModernRenderableScene();

	void UpdateRigidSceneObjects(const FrameDataUpdateInfo& frameUpdate, const std::span<ObjectDataUpdateInfo> rigidObjectUpdates) override final;

protected:
	uint64_t CalculateMaterialDataOffset(uint32_t materialIndex)                                           const;
	uint64_t CalculateRigidObjectDataOffset(uint32_t currentFrameResourceIndex, uint32_t rigidObjectIndex) const;
	uint64_t CalculateFrameDataOffset(uint32_t currentFrameResourceIndex)                                  const;

protected:
	//Created from inside
	const FrameCounter* mFrameCounterRef;

protected:
	//Created from outside
	std::vector<RigidObjectUpdateMetadata> mPrevFrameRigidMeshUpdates; //Sorted, ping-pongs with mNextFrameMeshUpdates
	std::vector<RigidObjectUpdateMetadata> mNextFrameRigidMeshUpdates; //Sorted, ping-pongs with mPrevFrameMeshUpdates

	std::vector<uint32_t> mCurrFrameRigidMeshUpdates; //Sorted, used immediately each frame for rendering

	std::vector<PerObjectData> mPrevFrameDataToUpdate;
	std::vector<PerObjectData> mCurrFrameDataToUpdate;


	void* mSceneConstantDataBufferPointer; //Constant buffer is persistently mapped

	uint32_t mObjectChunkDataSize;
	uint32_t mFrameChunkDataSize;
	uint32_t mMaterialChunkDataSize;
};