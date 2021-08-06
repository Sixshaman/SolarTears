#pragma once

#include "BaseRenderableScene.hpp"
#include <span>

//Class for scene functions common to Vulkan and D3D12
class ModernRenderableScene: public BaseRenderableScene
{
	friend class ModernRenderableSceneBuilder;

	struct RigidObjectUpdateMetadata
	{
		uint32_t MeshHandleIndex; //Unique for each instance of the mesh (NOT the index into mSceneMeshes)
		uint32_t ObjectDataIndex; //Index into mCurrFrameDataToUpdate
	};

public:
	ModernRenderableScene(uint64_t constantDataAlignment);
	~ModernRenderableScene();

	void UpdateFrameData(const FrameDataUpdateInfo& frameUpdate, uint64_t frameNumber)                      override final;
	void UpdateRigidSceneObjects(const std::span<ObjectDataUpdateInfo> objectUpdates, uint64_t frameNumber) override final;

protected:
	uint64_t CalculateMaterialDataOffset(uint32_t materialIndex)                                           const;
	uint64_t CalculateStaticObjectDataOffset(uint32_t staticObjectIndex)                                   const;
	uint64_t CalculateRigidObjectDataOffset(uint32_t currentFrameResourceIndex, uint32_t rigidObjectIndex) const;
	uint64_t CalculateFrameDataOffset(uint32_t currentFrameResourceIndex)                                  const;

	uint64_t GetBaseMaterialDataOffset()                                      const;
	uint64_t GetBaseStaticObjectDataOffset()                                  const;
	uint64_t GetBaseRigidObjectDataOffset(uint32_t currentFrameResourceIndex) const;
	uint64_t GetBaseFrameDataOffset(uint32_t currentFrameResourceIndex)       const;

protected:
	//Leftover updates to updates all dirty data for frames in flight. Sorted by mesh indices, ping-pong with each other
	std::vector<RigidObjectUpdateMetadata> mPrevFrameRigidMeshUpdates;
	std::vector<RigidObjectUpdateMetadata> mNextFrameRigidMeshUpdates;

	//The rigid object data indices to update in the current frame, sorted. Used immediately each frame for rendering after updating
	std::vector<uint32_t> mCurrFrameRigidMeshUpdateIndices;

	//The object data that is gonna be uploaded to GPU. The elements in mCurrFrameDataToUpdate correspond to elements in mCurrFrameRigidMeshUpdateIndices
	std::vector<PerObjectData> mPrevFrameDataToUpdate;
	std::vector<PerObjectData> mCurrFrameDataToUpdate;

	//Persistently mapped pointer into host-visible constant buffer data
	void* mSceneConstantDataBufferPointer;

	//GPU-local constant buffer data offsets
	uint64_t mMaterialDataSize;
	uint64_t mStaticObjectDataSize;

	//Single-object constant buffer sizes, with respect to alignment
	uint32_t mObjectChunkDataSize;
	uint32_t mFrameChunkDataSize;
	uint32_t mMaterialChunkDataSize;
};