#pragma once

#include <DirectXMath.h>

struct SceneObjectLocation
{
	DirectX::XMFLOAT3 Position;
	float             Scale;
	DirectX::XMFLOAT4 RotationQuaternion;

	void SetPrincipalRight(DirectX::XMVECTOR newRightNrm);
	void SetPrincipalUp(DirectX::XMVECTOR newUpNrm);
	void SetPrincipalForward(DirectX::XMVECTOR newForwardNrm);
};