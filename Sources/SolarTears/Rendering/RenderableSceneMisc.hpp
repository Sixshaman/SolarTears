#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "../Math/3rdParty/DirectXMath/Inc/DirectXMath.h"

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
	uint32_t Id;

	bool operator==(const RenderableSceneMeshHandle right) {return Id == right.Id;}
	bool operator!=(const RenderableSceneMeshHandle right) {return Id != right.Id;}
};

static constexpr RenderableSceneMeshHandle INVALID_SCENE_MESH_HANDLE = {.Id = (uint32_t)(-1)};