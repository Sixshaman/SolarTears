#include "Scene.hpp"
#include "../../Rendering/Common/Scene/BaseRenderableScene.hpp"
#include "../../Input/Inputter.hpp"

Scene::Scene()
{
}

Scene::~Scene()
{
}

void Scene::ProcessControls(Inputter* inputter, float dt)
{
	SceneObjectLocation& cameraLocation = mSceneObjects[(uint32_t)SpecialSceneObjects::Camera].Location;

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

void Scene::UpdateScene(uint64_t frameNumber)
{
	UpdateRenderableComponent(frameNumber);
}

void Scene::UpdateRenderableComponent(uint64_t frameNumber)
{
	//Update frame data
	FrameDataUpdateInfo frameUpdateInfo =
	{
		.CameraLocation = mSceneObjects[(uint32_t)SpecialSceneObjects::Camera].Location,
		.ProjMatrix     = mCamera.GetProjMatrix()
	};

	mRenderableComponentRef->UpdateFrameData(frameUpdateInfo, frameNumber);


	//No objects move currently
	std::span currentFrameMovedObjects = { mCurrFrameRenderableUpdates.begin(), mCurrFrameRenderableUpdates.begin() };

	mRenderableComponentRef->UpdateRigidSceneObjects(currentFrameMovedObjects, frameNumber); //Always needs to be called
}