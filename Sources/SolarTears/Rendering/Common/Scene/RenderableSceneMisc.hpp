#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <DirectXMath.h>

struct RenderableSceneObjectHandle
{
	uint64_t Id; //8 bits for object type, 56 bits for the index into the corresponding array

	bool operator==(const RenderableSceneObjectHandle right) const { return Id == right.Id; }
	bool operator!=(const RenderableSceneObjectHandle right) const { return Id != right.Id; }
	bool operator< (const RenderableSceneObjectHandle right) const { return Id <  right.Id; }
	bool operator> (const RenderableSceneObjectHandle right) const { return Id >  right.Id; }
};

static constexpr RenderableSceneObjectHandle INVALID_SCENE_OBJECT_HANDLE = {.Id = (uint64_t)(-1)};


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