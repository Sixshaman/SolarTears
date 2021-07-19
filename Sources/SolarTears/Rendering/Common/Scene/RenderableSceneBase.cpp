#include "RenderableSceneBase.hpp"

RenderableSceneBase::RenderableSceneBase(uint32_t maxDirtyFrames)
{
}

RenderableSceneBase::~RenderableSceneBase()
{
}

RenderableSceneBase::PerObjectData RenderableSceneBase::PackObjectData(const LocationComponent& sceneObjectLocation) const
{
	const DirectX::XMVECTOR scale    = DirectX::XMVectorSet(sceneObjectLocation.Scale, sceneObjectLocation.Scale, sceneObjectLocation.Scale, 0.0f);
	const DirectX::XMVECTOR rotation = DirectX::XMLoadFloat4(&sceneObjectLocation.RotationQuaternion);
	const DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&sceneObjectLocation.Position);
	const DirectX::XMVECTOR zeroVec  = DirectX::XMVectorZero();

	DirectX::XMFLOAT4X4 objectMatrix;
	DirectX::XMStoreFloat4x4(&objectMatrix, DirectX::XMMatrixAffineTransformation(scale, zeroVec, rotation, position));

	return RenderableSceneBase::PerObjectData
	{
		.WorldMatrix = objectMatrix
	};
}

RenderableSceneBase::PerFrameData RenderableSceneBase::PackFrameData(DirectX::FXMMATRIX View, DirectX::FXMMATRIX Proj) const
{
	DirectX::XMFLOAT4X4 viewProj;
	DirectX::XMStoreFloat4x4(&viewProj, DirectX::XMMatrixMultiply(View, Proj));

	return RenderableSceneBase::PerFrameData
	{
		.ViewProjMatrix = viewProj
	};
}

size_t RenderableSceneBase::ObjectArrayIndex(const RenderableSceneMeshHandle meshHandle)
{
	return meshHandle.Id & 0x00ffffffffffffffull;
}

uint8_t RenderableSceneBase::ObjectType(const RenderableSceneMeshHandle meshHandle)
{
	return uint8_t(meshHandle.Id & 0xff00000000000000ull) >> 56;
}
