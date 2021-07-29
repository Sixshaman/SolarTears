#include "BaseRenderableScene.hpp"

BaseRenderableScene::BaseRenderableScene()
{
}

BaseRenderableScene::~BaseRenderableScene()
{
}

BaseRenderableScene::PerObjectData BaseRenderableScene::PackObjectData(const SceneObjectLocation& sceneObjectLocation) const
{
	const DirectX::XMVECTOR scale    = DirectX::XMVectorSet(sceneObjectLocation.Scale, sceneObjectLocation.Scale, sceneObjectLocation.Scale, 0.0f);
	const DirectX::XMVECTOR rotation = DirectX::XMLoadFloat4(&sceneObjectLocation.RotationQuaternion);
	const DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&sceneObjectLocation.Position);
	const DirectX::XMVECTOR zeroVec  = DirectX::XMVectorZero();

	DirectX::XMFLOAT4X4 objectMatrix;
	DirectX::XMStoreFloat4x4(&objectMatrix, DirectX::XMMatrixAffineTransformation(scale, zeroVec, rotation, position));

	return BaseRenderableScene::PerObjectData
	{
		.WorldMatrix = objectMatrix
	};
}

BaseRenderableScene::PerFrameData BaseRenderableScene::PackFrameData(DirectX::FXMMATRIX View, DirectX::FXMMATRIX Proj) const
{
	DirectX::XMFLOAT4X4 viewProj;
	DirectX::XMStoreFloat4x4(&viewProj, DirectX::XMMatrixMultiply(View, Proj));

	return BaseRenderableScene::PerFrameData
	{
		.ViewProjMatrix = viewProj
	};
}