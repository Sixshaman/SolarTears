#pragma once

#include "RenderableSceneMisc.hpp"

struct RenderableSceneMaterialData
{
	std::wstring TextureFilename;
};

struct RenderableSceneGeometryData
{
	std::vector<RenderableSceneVertex> Vertices;
	std::vector<RenderableSceneIndex>  Indices;
};

struct RenderableSceneMeshData
{
	std::string GeometryName;
	std::string MaterialName;
};