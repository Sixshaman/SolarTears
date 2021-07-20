#pragma once

#include "RenderableSceneMisc.hpp"

struct RenderableSceneStaticMeshDescription
{
	std::vector<RenderableSceneVertex> Vertices;
	std::vector<RenderableSceneIndex>  Indices;
	std::wstring                       TextureFilename;
};

struct RenderableSceneRigidMeshDescription
{
	std::vector<RenderableSceneVertex> Vertices;
	std::vector<RenderableSceneIndex>  Indices;
	std::wstring                       TextureFilename;

	SceneObjectLocation InitialLocation;
};