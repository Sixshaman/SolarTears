#pragma once

#include <vector>
#include <string>
#include <unordered_set>
#include "RenderableSceneBase.hpp"
#include "RenderableSceneMisc.hpp"
#include <DirectXMath.h>

class RenderableSceneBuilderBase
{
public:
	RenderableSceneBuilderBase(RenderableSceneBase* sceneToBuild);
	~RenderableSceneBuilderBase();

	//Can't remove meshes for now, the scene is supposed to be static
	//To implement mesh removal, RenderableSceneMeshHandle should be much more complex id
	RenderableSceneMeshHandle AddSceneMesh(const RenderableSceneMesh& sceneMesh);

protected:
	RenderableSceneBase* mSceneToBuild;

	std::vector<RenderableSceneMesh> mSceneStaticMeshes;
	std::vector<RenderableSceneMesh> mSceneRigidMeshes;

	std::unordered_set<std::wstring> mSceneTextures;
};