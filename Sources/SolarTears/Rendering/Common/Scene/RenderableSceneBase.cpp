#include "RenderableSceneBase.hpp"

RenderableSceneBase::RenderableSceneBase(uint32_t maxDirtyFrames)
{
}

RenderableSceneBase::~RenderableSceneBase()
{
}

void RenderableSceneBase::CalculatePerObjectData(const SceneObjectLocation& sceneObjectLocation, PerObjectData* outPerObjectData)
{
	assert(outPerObjectData != nullptr);
	
	const DirectX::XMVECTOR scale    = DirectX::XMVectorSet(sceneObjectLocation.Scale, sceneObjectLocation.Scale, sceneObjectLocation.Scale, 0.0f);
	const DirectX::XMVECTOR rotation = DirectX::XMLoadFloat4(&sceneObjectLocation.RotationQuaternion);
	const DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&sceneObjectLocation.Position);
	const DirectX::XMVECTOR zeroVec  = DirectX::XMVectorZero();

	const DirectX::XMMATRIX objectMatrix = DirectX::XMMatrixAffineTransformation(scale, zeroVec, rotation, position);
	DirectX::XMStoreFloat4x4(&outPerObjectData->WorldMatrix, DirectX::XMMatrixTranspose(objectMatrix));
}

void RenderableSceneBase::UpdateSceneCameraData(DirectX::FXMMATRIX View, DirectX::FXMMATRIX Proj, PerFrameData* outPerFrameData)
{
	assert(outPerFrameData != nullptr);

	DirectX::XMMATRIX viewProj = DirectX::XMMatrixMultiply(View, Proj);
	DirectX::XMStoreFloat4x4(&outPerFrameData->ViewProjMatrix, viewProj);
}
