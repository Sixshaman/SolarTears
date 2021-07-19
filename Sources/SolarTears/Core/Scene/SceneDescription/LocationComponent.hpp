#pragma once

#include <DirectXMath.h>

struct LocationComponent
{
	DirectX::XMFLOAT3 Position;
	float             Scale;
	DirectX::XMFLOAT4 RotationQuaternion;
};