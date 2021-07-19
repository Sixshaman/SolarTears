#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <DirectXMath.h>

struct RenderableSceneVertex
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 Texcoord;
};

using RenderableSceneIndex = uint32_t;

struct RenderableSceneMesh
{
	std::vector<RenderableSceneVertex> Vertices;
	std::vector<RenderableSceneIndex>  Indices;
	std::wstring                       TextureFilename;
};

struct RenderableSceneMeshHandle
{
	uint64_t Id; //8 bits for object type, 56 bits for the index into the corresponding array

	bool operator==(const RenderableSceneMeshHandle right) const { return Id == right.Id; }
	bool operator!=(const RenderableSceneMeshHandle right) const { return Id != right.Id; }
	bool operator< (const RenderableSceneMeshHandle right) const { return Id <  right.Id; }
	bool operator> (const RenderableSceneMeshHandle right) const { return Id >  right.Id; }
};

static constexpr RenderableSceneMeshHandle INVALID_SCENE_MESH_HANDLE = {.Id = (uint64_t)(-1)};