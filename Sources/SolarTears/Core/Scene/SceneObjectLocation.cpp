#include "SceneObjectLocation.hpp"
#include "../Math/QuaternionUtils.hpp"

void SceneObjectLocation::SetPrincipalRight(DirectX::XMVECTOR right)
{
	DirectX::XMVECTOR defaultRight = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	DirectX::XMStoreFloat4(&RotationQuaternion, Utils::QuaternionBetweenTwoVectorsNormalized(defaultRight, right));
}

void SceneObjectLocation::SetPrincipalUp(DirectX::XMVECTOR newUpNrm)
{
	DirectX::XMVECTOR defaultUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	DirectX::XMStoreFloat4(&RotationQuaternion, Utils::QuaternionBetweenTwoVectorsNormalized(defaultUp, newUpNrm));
}

void SceneObjectLocation::SetPrincipalForward(DirectX::XMVECTOR newForwardNrm)
{
	DirectX::XMVECTOR defaultForward = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	DirectX::XMStoreFloat4(&RotationQuaternion, Utils::QuaternionBetweenTwoVectorsNormalized(defaultForward, newForwardNrm));
}