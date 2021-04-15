#pragma once

#include <DirectXMath.h>

struct SceneObjectLocation
{
	DirectX::XMFLOAT3 Position;
	float             Scale;
	DirectX::XMFLOAT4 RotationQuaternion;
};