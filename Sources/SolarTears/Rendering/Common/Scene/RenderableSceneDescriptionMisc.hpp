#pragma once

#include "RenderableSceneMisc.hpp"

enum class RenderableSceneMeshFlags: uint32_t
{
	NonStatic = 0x01,
};

struct RenderableSceneMaterialData
{
	std::wstring TextureFilename;
};

struct RenderableSceneGeometryData
{
	std::vector<RenderableSceneVertex> Vertices;
	std::vector<RenderableSceneIndex>  Indices;
};

struct RenderableSceneSubmeshData
{
	std::string GeometryName;
	std::string MaterialName;
};

struct RenderableSceneMeshData
{
	std::vector<RenderableSceneSubmeshData> Submeshes;
	uint32_t                                MeshFlags;
};