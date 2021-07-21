#pragma once

#include "RenderableSceneMisc.hpp"

struct RenderableSceneMeshData
{
	std::vector<RenderableSceneVertex> Vertices;
	std::vector<RenderableSceneIndex>  Indices;
	std::wstring                       TextureFilename;
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