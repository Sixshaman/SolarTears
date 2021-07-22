#pragma once

#include <DirectXMath.h>

namespace Utils
{
	//Calculates a quaternion that rotates between two unit vectors a -> b
	DirectX::XMVECTOR XM_CALLCONV QuaternionBetweenTwoVectorsNormalized(DirectX::FXMVECTOR a, DirectX::FXMVECTOR b);
}