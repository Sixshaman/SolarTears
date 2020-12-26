#include "RenderableSceneBase.hpp"

RenderableSceneBase::RenderableSceneBase(uint32_t maxDirtyFrames): mMaxDirtyFrames(maxDirtyFrames)
{
	mScheduledSceneUpdatesCount = 0;
}

RenderableSceneBase::~RenderableSceneBase()
{
}

void RenderableSceneBase::UpdateSceneMeshData(RenderableSceneMeshHandle meshHandle, const Scene::SceneObjectLocation& sceneObjectLocation)
{
	if(mFrameDataScheduledUpdateIndex == -1)
	{
		ScheduledSceneUpdate sceneUpdate;
		sceneUpdate.UpdateType       = SceneUpdateType::UPDATE_OBJECT;
		sceneUpdate.ObjectIndex      = meshHandle.Id;
		sceneUpdate.DirtyFramesCount = mMaxDirtyFrames;

		mScheduledSceneUpdates[mScheduledSceneUpdatesCount] = sceneUpdate;
		mScheduledSceneUpdatesCount                         = mScheduledSceneUpdatesCount + 1;
	}
	else
	{
		mScheduledSceneUpdates[mObjectDataScheduledUpdateIndices[meshHandle.Id]].DirtyFramesCount = mMaxDirtyFrames;
	}

	const DirectX::XMVECTOR scale    = DirectX::XMVectorSet(sceneObjectLocation.Scale, sceneObjectLocation.Scale, sceneObjectLocation.Scale, 0.0f);
	const DirectX::XMVECTOR rotation = DirectX::XMLoadFloat4(&sceneObjectLocation.RotationQuaternion);
	const DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&sceneObjectLocation.Position);
	const DirectX::XMVECTOR zeroVec  = DirectX::XMVectorZero();

	const DirectX::XMMATRIX objectMatrix = DirectX::XMMatrixAffineTransformation(scale, zeroVec, rotation, position);
	DirectX::XMStoreFloat4x4(&mScenePerObjectData[meshHandle.Id].WorldMatrix, DirectX::XMMatrixTranspose(objectMatrix));

	mObjectDataScheduledUpdateIndices[meshHandle.Id] = mScheduledSceneUpdatesCount - 1;
}

void RenderableSceneBase::UpdateSceneCameraData(DirectX::XMMATRIX View, DirectX::XMMATRIX Proj)
{
	if(mFrameDataScheduledUpdateIndex == -1)
	{
		ScheduledSceneUpdate sceneUpdate;
		sceneUpdate.UpdateType       = SceneUpdateType::UPDATE_COMMON;
		sceneUpdate.ObjectIndex      = (uint32_t)(-1);
		sceneUpdate.DirtyFramesCount = mMaxDirtyFrames;

		mScheduledSceneUpdates[mScheduledSceneUpdatesCount] = sceneUpdate;
		mScheduledSceneUpdatesCount                         = mScheduledSceneUpdatesCount + 1;
	}
	else
	{
		mScheduledSceneUpdates[mFrameDataScheduledUpdateIndex].DirtyFramesCount = mMaxDirtyFrames;
	}

	DirectX::XMMATRIX viewProj = DirectX::XMMatrixMultiply(View, Proj);
	DirectX::XMStoreFloat4x4(&mScenePerFrameData.ViewProjMatrix, viewProj);

	mFrameDataScheduledUpdateIndex = mScheduledSceneUpdatesCount - 1;
}
