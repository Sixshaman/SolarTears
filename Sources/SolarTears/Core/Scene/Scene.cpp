#include "Scene.hpp"
#include "../../Rendering/Common/Scene/RenderableSceneBase.hpp"
#include "../../Input/Inputter.hpp"

Scene::Scene()
{
}

Scene::~Scene()
{
}

void Scene::ProcessControls(Inputter* inputter, float dt)
{
	SceneObjectLocation& cameraLocation = mSceneObjects[mCameraSceneObjectIndex].Location;

	DirectX::XMVECTOR cameraQuaternion = DirectX::XMLoadFloat4(&cameraLocation.RotationQuaternion);
	DirectX::XMMATRIX cameraRotation   = DirectX::XMMatrixRotationQuaternion(cameraQuaternion);

	DirectX::XMVECTOR cameraPosition = DirectX::XMLoadFloat3(&cameraLocation.Position);

	DirectX::XMVECTOR pitchAxis = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), cameraRotation);
	DirectX::XMVECTOR yawAxis   = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), cameraRotation);
	DirectX::XMVECTOR rollAxis  = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), cameraRotation);

	DirectX::XMVECTOR moveVector = DirectX::XMVectorZero();

	if(inputter->GetKeyState(ControlCode::MoveForward))
	{
		moveVector = DirectX::XMVectorAdd(moveVector, rollAxis);
	}

	if(inputter->GetKeyState(ControlCode::MoveBack))
	{
		moveVector = DirectX::XMVectorSubtract(moveVector, rollAxis);
	}

	if(inputter->GetKeyState(ControlCode::MoveRight))
	{
		moveVector = DirectX::XMVectorAdd(moveVector, pitchAxis);
	}

	if(inputter->GetKeyState(ControlCode::MoveLeft))
	{
		moveVector = DirectX::XMVectorSubtract(moveVector, pitchAxis);
	}

	moveVector = DirectX::XMVector3Normalize(moveVector);
	moveVector = DirectX::XMVectorScale(moveVector, 10.0f * dt);

	cameraPosition = DirectX::XMVectorAdd(cameraPosition, moveVector);
	DirectX::XMStoreFloat3(&cameraLocation.Position, cameraPosition);


	DirectX::XMFLOAT2 axisDelta = inputter->GetAxis2Delta();

	float rotationFactorXSin = sinf(axisDelta.x * dt / 2.0f);
	float rotationFactorXCos = cosf(axisDelta.x * dt / 2.0f);

	float rotationFactorYSin = sinf(-axisDelta.y * dt / 2.0f);
	float rotationFactorYCos = cosf(-axisDelta.y * dt / 2.0f);

	DirectX::XMVECTOR rotationFactorVecX  = DirectX::XMVectorSet(rotationFactorXSin, rotationFactorXSin, rotationFactorXSin, 0.0f);
	DirectX::XMVECTOR rotationQuaternionX = DirectX::XMVectorSetW(DirectX::XMVectorMultiply(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rotationFactorVecX), rotationFactorXCos);

	DirectX::XMVECTOR rotationFactorVecY  = DirectX::XMVectorSet(rotationFactorYSin, rotationFactorYSin, rotationFactorYSin, 0.0f);
	DirectX::XMVECTOR rotationQuaternionY = DirectX::XMVectorSetW(DirectX::XMVectorMultiply(pitchAxis, rotationFactorVecY), rotationFactorYCos);

	DirectX::XMVECTOR resultQuaternion = DirectX::XMLoadFloat4(&cameraLocation.RotationQuaternion);
	resultQuaternion = DirectX::XMQuaternionMultiply(resultQuaternion, rotationQuaternionY);
	resultQuaternion = DirectX::XMQuaternionMultiply(resultQuaternion, rotationQuaternionX);

	DirectX::XMStoreFloat4(&cameraLocation.RotationQuaternion, resultQuaternion);
}

void Scene::UpdateScene()
{
	UpdateRenderableComponent();
}

void Scene::UpdateRenderableComponent()
{
	for(size_t i = 0; i < mSceneObjects.size(); i++)
	{
		if(mSceneObjects[i].RenderableHandle != INVALID_SCENE_OBJECT_HANDLE)
		{
			mRenderableComponentRef->UpdateSceneMeshData(mSceneObjects[i].RenderableHandle, mSceneObjects[i].Location);
			mSceneObjects[i].DirtyFlag = false;
		}
	}

	//The matrix from camera space to world space
	DirectX::XMVECTOR cameraPosition       = DirectX::XMLoadFloat3(&mSceneObjects[mCameraSceneObjectIndex].Location.Position);
	DirectX::XMVECTOR cameraRotation       = DirectX::XMLoadFloat4(&mSceneObjects[mCameraSceneObjectIndex].Location.RotationQuaternion);
	DirectX::XMVECTOR cameraRotationOrigin = DirectX::XMVectorZero();
	DirectX::XMVECTOR cameraScale          = DirectX::XMVectorSplatOne();
	DirectX::XMMATRIX invViewMatrix = DirectX::XMMatrixAffineTransformation(cameraScale, cameraRotationOrigin, cameraRotation, cameraPosition);

	mCamera.RecalcProjMatrix();
	FrameDataUpdateInfo frameUpdateInfo =
	{
		.ViewMatrix = DirectX::XMMatrixInverse(nullptr, invViewMatrix),
		.ProjMatrix = mCamera.GetProjMatrix()
	};

	mRenderableComponentRef->UpdateRigidSceneObjects(frameUpdateInfo, mCurrFrameRenderableUpdates);
}