#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <DirectXMath.h>
#include "../../../Core/Scene/SceneObjectLocation.hpp"

using RenderableSceneObjectHandle = uint32_t;


struct RenderableSceneVertex
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 Texcoord;
};

using RenderableSceneIndex = uint32_t;


struct RenderableSceneMaterial
{
	uint32_t TextureIndex;
	uint32_t NormalMapIndex;
};


struct ObjectDataUpdateInfo
{
	RenderableSceneObjectHandle ObjectId;
	SceneObjectLocation         NewObjectLocation;
};

struct FrameDataUpdateInfo
{
	SceneObjectLocation CameraLocation;
	DirectX::XMFLOAT4X4 ProjMatrix;
};