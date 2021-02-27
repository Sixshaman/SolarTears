#pragma once

#include "../../../3rd party/DirectXMath/Inc/DirectXMath.h"

struct SceneObjectLocation
{
	DirectX::XMFLOAT3 Position;
	float             Scale;
	DirectX::XMFLOAT4 RotationQuaternion;
};