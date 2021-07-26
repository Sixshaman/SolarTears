#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <DirectXMath.h>
#include "RenderableSceneObjectHandle.hpp"
#include "../../../Core/Scene/SceneObjectLocation.hpp"

using RenderableSceneObjectHandle = RenderableSceneObjectHandleGeneral<uint32_t>;


struct RenderableSceneVertex
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 Texcoord;
};

using RenderableSceneIndex = uint32_t;


struct ObjectDataUpdateInfo
{
	RenderableSceneObjectHandle ObjectId;
	SceneObjectLocation         NewObjectLocation;
};

struct alignas(DirectX::XMMATRIX) FrameDataUpdateInfo
{
	DirectX::XMMATRIX ViewMatrix;
	DirectX::XMMATRIX ProjMatrix;
};