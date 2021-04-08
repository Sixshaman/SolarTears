#pragma once

#include "../../3rd party/DirectXMath/Inc/DirectXMath.h"

struct ControlState
{
	DirectX::XMFLOAT2 Axis1Delta;
	DirectX::XMFLOAT2 Axis2Delta;
	DirectX::XMFLOAT2 Axis3Delta;
	uint32_t          CurrentKeyStates;
	uint32_t          KeyStateChangeFlags;
};