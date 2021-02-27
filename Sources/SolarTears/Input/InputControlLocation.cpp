#include "InputControlLocation.hpp"

InputControlLocation::InputControlLocation(const SceneObjectLocation& sceneObjectLocation): mCurrentLocation(sceneObjectLocation)
{
	DirectX::XMVECTOR objectQuaternion = DirectX::XMLoadFloat4(&sceneObjectLocation.RotationQuaternion);
	DirectX::XMMATRIX objectRotation   = DirectX::XMMatrixRotationQuaternion(objectQuaternion);

	DirectX::XMVECTOR pitchAxis = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), objectRotation);
	DirectX::XMVECTOR yawAxis   = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), objectRotation);
	DirectX::XMVECTOR rollAxis  = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), objectRotation);

	DirectX::XMStoreFloat3(&mPitchAxis, pitchAxis);
	DirectX::XMStoreFloat3(&mYawAxis,   yawAxis);
	DirectX::XMStoreFloat3(&mRollAxis,  rollAxis);
}

InputControlLocation::~InputControlLocation()
{
}

void InputControlLocation::Walk(float d)
{
	DirectX::XMVECTOR currentPosition = DirectX::XMLoadFloat3(&mCurrentLocation.Position);
	DirectX::XMVECTOR direction       = DirectX::XMLoadFloat3(&mRollAxis);
	DirectX::XMVECTOR factor          = DirectX::XMVectorSet(d, d, d, 0.0f);

	DirectX::XMVECTOR newPosition = DirectX::XMVectorMultiplyAdd(direction, factor, currentPosition);
	DirectX::XMStoreFloat3(&mCurrentLocation.Position, newPosition);
}

void InputControlLocation::Strafe(float d)
{
	DirectX::XMVECTOR currentPosition = DirectX::XMLoadFloat3(&mCurrentLocation.Position);
	DirectX::XMVECTOR direction       = DirectX::XMLoadFloat3(&mPitchAxis);
	DirectX::XMVECTOR factor          = DirectX::XMVectorSet(d, d, d, 0.0f);

	DirectX::XMVECTOR newPosition = DirectX::XMVectorMultiplyAdd(direction, factor, currentPosition);
	DirectX::XMStoreFloat3(&mCurrentLocation.Position, newPosition);
}

void InputControlLocation::Float(float d)
{
	DirectX::XMVECTOR currentPosition = DirectX::XMLoadFloat3(&mCurrentLocation.Position);
	DirectX::XMVECTOR direction       = DirectX::XMLoadFloat3(&mYawAxis);
	DirectX::XMVECTOR factor          = DirectX::XMVectorSet(d, d, d, 0.0f);

	DirectX::XMVECTOR newPosition = DirectX::XMVectorMultiplyAdd(direction, factor, currentPosition);
	DirectX::XMStoreFloat3(&mCurrentLocation.Position, newPosition);
}

void InputControlLocation::Pitch(float d)
{
	DirectX::XMVECTOR rotationAxis = DirectX::XMLoadFloat3(&mPitchAxis);
	Rotate(rotationAxis, d);
}

void InputControlLocation::Yaw(float d)
{
	DirectX::XMVECTOR rotationAxis = DirectX::XMLoadFloat3(&mYawAxis);
	Rotate(rotationAxis, d);
}

void InputControlLocation::Roll(float d)
{
	DirectX::XMVECTOR rotationAxis = DirectX::XMLoadFloat3(&mRollAxis);
	Rotate(rotationAxis, d);
}

SceneObjectLocation InputControlLocation::GetLocation() const
{
	return mCurrentLocation;
}

void InputControlLocation::Rotate(DirectX::XMVECTOR rotationAxis, float d)
{
	float rotationFactorSin = sinf(d / 2.0f);
	float rotationFactorCos = cosf(d / 2.0f);

	DirectX::XMVECTOR rotationFactorVec  = DirectX::XMVectorSet(rotationFactorSin, rotationFactorSin, rotationFactorSin, 0.0f);
	DirectX::XMVECTOR rotationQuaternion = DirectX::XMVectorSetW(DirectX::XMVectorMultiply(rotationAxis, rotationFactorVec), rotationFactorCos);
	
	DirectX::XMVECTOR resultQuaternion = DirectX::XMQuaternionMultiply(DirectX::XMLoadFloat4(&mCurrentLocation.RotationQuaternion), rotationQuaternion);
	DirectX::XMStoreFloat4(&mCurrentLocation.RotationQuaternion, resultQuaternion);

	DirectX::XMMATRIX objectRotation = DirectX::XMMatrixRotationQuaternion(resultQuaternion);

	DirectX::XMVECTOR pitchAxis = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), objectRotation);
	DirectX::XMVECTOR yawAxis   = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), objectRotation);
	DirectX::XMVECTOR rollAxis  = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), objectRotation);

	DirectX::XMStoreFloat3(&mPitchAxis, pitchAxis);
	DirectX::XMStoreFloat3(&mYawAxis,   yawAxis);
	DirectX::XMStoreFloat3(&mRollAxis,  rollAxis);
}
