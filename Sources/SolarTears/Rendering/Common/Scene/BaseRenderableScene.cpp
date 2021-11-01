#include "BaseRenderableScene.hpp"

BaseRenderableScene::BaseRenderableScene()
{
	mStaticUniqueMeshSpan.Begin = 0;
	mStaticUniqueMeshSpan.End   = 0;

	mNonStaticMeshSpan.Begin = 0;
	mNonStaticMeshSpan.End   = 0;
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

BaseRenderableScene::PerFrameData BaseRenderableScene::PackFrameData(const SceneObjectLocation& cameraLocation, DirectX::FXMMATRIX ProjMatrix) const
{
	//The matrix from camera space to world space
	DirectX::XMVECTOR cameraPosition       = DirectX::XMLoadFloat3(&cameraLocation.Position);
	DirectX::XMVECTOR cameraRotation       = DirectX::XMLoadFloat4(&cameraLocation.RotationQuaternion);
	DirectX::XMVECTOR cameraRotationOrigin = DirectX::XMVectorZero();
	DirectX::XMVECTOR cameraScale          = DirectX::XMVectorSplatOne();

	DirectX::XMMATRIX invViewMatrix = DirectX::XMMatrixAffineTransformation(cameraScale, cameraRotationOrigin, cameraRotation, cameraPosition);
	DirectX::XMMATRIX viewMatrix    = DirectX::XMMatrixInverse(nullptr, invViewMatrix);

	DirectX::XMFLOAT4X4 viewProj;
	DirectX::XMStoreFloat4x4(&viewProj, DirectX::XMMatrixMultiply(viewMatrix, ProjMatrix));

	return BaseRenderableScene::PerFrameData
	{
		.ViewProjMatrix = viewProj
	};
}