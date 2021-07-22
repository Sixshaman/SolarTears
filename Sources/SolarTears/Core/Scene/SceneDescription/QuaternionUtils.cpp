#include "QuaternionUtils.hpp"

DirectX::XMVECTOR XM_CALLCONV Utils::QuaternionBetweenTwoVectorsNormalized(DirectX::FXMVECTOR a, DirectX::FXMVECTOR b)
{
	[[unlikely]]
	if(DirectX::XMVector3NearEqual(a, DirectX::XMVectorNegate(b), DirectX::XMVectorSplatConstant(1, 16)))
	{
		//180 degree rotation about an arbitrary axis
		return DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, -1.0f);
	}
	else
	{
		//Find a quaternion that rotates a -> b
		
		//Calculate the necessary rotation quaternion q = (n * sin(theta / 2), cos(theta / 2)) that rotates by angle theta.
		//We have rotAxis = n * sin(theta). We can construct a quaternion q2 = (n * sin(theta), cos(theta)) that rotates by angle 2 * theta. 
		//Then we construct a quaternion q0 = (0, 0, 0, 1) that rotates by angle 0. Then q = normalize((q2 + q0) / 2) = normalize(q2 + q0).
		//No slerp needed since it's a perfect mid-way interpolation.
		const DirectX::XMVECTOR rotAxis  = DirectX::XMVector3Cross(a, b);
		const DirectX::XMVECTOR cosAngle = DirectX::XMVector3Dot(a, b);

		const DirectX::XMVECTOR unitQuaternion  = DirectX::XMQuaternionIdentity();
		const DirectX::XMVECTOR twiceQuaternion = DirectX::XMVectorPermute<XM_PERMUTE_0X, XM_PERMUTE_0Y, XM_PERMUTE_0Z, XM_PERMUTE_1X>(rotAxis, cosAngle);

		return DirectX::XMQuaternionNormalize(DirectX::XMVectorAdd(unitQuaternion, twiceQuaternion));
	}
}