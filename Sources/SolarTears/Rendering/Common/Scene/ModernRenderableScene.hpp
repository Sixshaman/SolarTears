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

	void UpdateFrameData(const FrameDataUpdateInfo& frameUpdate, uint64_t frameNumber)                           override final;
	void UpdateRigidSceneObjects(const std::span<ObjectDataUpdateInfo> rigidObjectUpdates, uint64_t frameNumber) override final;

protected:
	uint64_t GetMaterialDataOffset(uint32_t materialIndex) const;
	uint64_t GetObjectDataOffset(uint32_t objectIndex)     const;

	uint64_t GetBaseMaterialDataOffset() const;
	uint64_t GetBaseFrameDataOffset()    const;
	uint64_t GetBaseObjectDataOffset()   const;

	uint64_t GetDynamicFrameDataOffset(uint32_t frameResourceIndex) const;
	uint64_t GetDynamicRigidObjectDataOffset(uint32_t frameResourceIndex, uint32_t rigidObjectDataIndex) const;

	uint32_t GetStaticObjectCount() const;
	uint32_t GetRigidObjectCount()  const;

protected:
	//Leftover updates to update all dirty data for frames in flight. Sorted by mesh indices, ping-pong with each other
	std::vector<RigidObjectUpdateMetadata> mPrevFrameRigidMeshUpdates;
	std::vector<RigidObjectUpdateMetadata> mNextFrameRigidMeshUpdates;

	//The rigid object data indices to update in the current frame, sorted. Used immediately each frame for rendering after updating
	std::vector<uint32_t> mCurrFrameRigidMeshUpdateIndices;

	//The object data that is gonna be uploaded to GPU. The elements in mCurrFrameDataToUpdate correspond to elements in mCurrFrameRigidMeshUpdateIndices
	std::vector<PerObjectData> mCurrFrameDataToUpdate;

	//Leftover updates from the previous frame
	std::vector<PerObjectData> mPrevFrameDataToUpdate;

	//Persistently mapped pointer into host-visible constant buffer data
	void* mSceneConstantDataBufferPointer;

	//GPU-local constant buffer data sizes
	uint64_t mMaterialDataSize;
	uint64_t mFrameDataSize;
	uint64_t mStaticObjectDataSize;
	uint64_t mRigidObjectDataSize;

	//Single-object constant buffer sizes, with respect to alignment
	uint32_t mObjectChunkDataSize;
	uint32_t mFrameChunkDataSize;
	uint32_t mMaterialChunkDataSize;
};