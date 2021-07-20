#pragma once

#include <vector>
#include <string>
#include <unordered_set>
#include "RenderableSceneBase.hpp"
#include "RenderableSceneBuilderMisc.hpp"
#include <DirectXMath.h>

class RenderableSceneBuilderBase
{
public:
	RenderableSceneBuilderBase(RenderableSceneBase* sceneToBuild);
	~RenderableSceneBuilderBase();

	RenderableSceneObjectHandle AddStaticSceneMesh(const RenderableSceneStaticMeshDescription& sceneMesh);
	RenderableSceneObjectHandle AddRigidSceneMesh(const RenderableSceneRigidMeshDescription& sceneMesh);

protected:
	RenderableSceneObjectHandle PackObjectInfo(RenderableSceneBase::SceneObjectType objectType, uint32_t objectArrayIndex);

protected:
	RenderableSceneBase* mSceneToBuild;

	std::vector<RenderableSceneStaticMeshDescription> mSceneStaticMeshes;
	std::vector<RenderableSceneRigidMeshDescription>  mSceneRigidMeshes;

	std::unordered_set<std::wstring> mSceneTextures;
};