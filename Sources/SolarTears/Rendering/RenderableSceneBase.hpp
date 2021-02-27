#pragma once

#include "../../3rd party/DirectXMath/Inc/DirectXMath.h"
#include "../Core/Scene/Scene.hpp"
#include "RenderableSceneMisc.hpp"

class RenderableSceneBase
{
protected:
	struct PerObjectData
	{
		DirectX::XMFLOAT4X4 WorldMatrix;
	};

	struct PerFrameData
	{
		DirectX::XMFLOAT4X4 ViewProjMatrix;
	};

	enum class SceneUpdateType
	{
		UPDATE_UNDEFINED, //Nothing
		UPDATE_OBJECT,    //Per-object data
		UPDATE_COMMON     //Per-frame data
	};

	struct ScheduledSceneUpdate
	{
		SceneUpdateType UpdateType;       //The type of update
		uint32_t        ObjectIndex;      //Object index to update from. -1 if UpdateType is UPDATE_COMMON
		uint32_t        DirtyFramesCount; //Dirty frames count
	};

public:
	RenderableSceneBase(uint32_t maxDirtyFrames);
	~RenderableSceneBase();

	void UpdateSceneMeshData(RenderableSceneMeshHandle meshHandle, const SceneObjectLocation& sceneObjectLocation);
	void UpdateSceneCameraData(DirectX::XMMATRIX View, DirectX::XMMATRIX Proj);

	virtual void FinalizeSceneUpdating() = 0;

protected:
	//Set from inside
	uint32_t mMaxDirtyFrames;

protected:
	//Set from outside
	std::vector<PerObjectData> mScenePerObjectData;
	PerFrameData               mScenePerFrameData;

	uint32_t                          mScheduledSceneUpdatesCount;
	std::vector<ScheduledSceneUpdate> mScheduledSceneUpdates;

	//TODO: potentially bad cache performance
	std::vector<uint32_t> mObjectDataScheduledUpdateIndices;
	uint32_t              mFrameDataScheduledUpdateIndex;
};