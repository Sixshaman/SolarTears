#pragma once

#include "RenderableSceneMisc.hpp"

struct RenderableSceneMaterialData
{
	std::wstring TextureFilename;
};

struct RenderableSceneMeshData
{
	std::vector<RenderableSceneVertex> Vertices;
	std::vector<RenderableSceneIndex>  Indices;
	std::string                        MaterialName;
};

struct RenderableSceneStaticMeshDescription
{
	RenderableSceneMeshData MeshDataDescription;
};

struct RenderableSceneRigidMeshDescription
{
	RenderableSceneMeshData MeshDataDescription;
	SceneObjectLocation     InitialLocation;
};