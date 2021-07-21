#include "RenderableSceneBuilderBase.hpp"

RenderableSceneBuilderBase::RenderableSceneBuilderBase(RenderableSceneBase* sceneToBuild): mSceneToBuild(sceneToBuild)
{
}

RenderableSceneBuilderBase::~RenderableSceneBuilderBase()
{
}

RenderableSceneObjectHandle RenderableSceneBuilderBase::AddStaticSceneMesh(const RenderableSceneStaticMeshDescription& sceneMesh)
{
	mSceneStaticMeshes.push_back(sceneMesh);
	mSceneTextures.insert(sceneMesh.MeshDataDescription.TextureFilename);

	return PackObjectInfo(RenderableSceneBase::SceneObjectType::Static, mSceneStaticMeshes.size() - 1);
}

RenderableSceneObjectHandle RenderableSceneBuilderBase::AddRigidSceneMesh(const RenderableSceneRigidMeshDescription& sceneMesh)
{
	mSceneRigidMeshes.push_back(sceneMesh);
	mSceneTextures.insert(sceneMesh.MeshDataDescription.TextureFilename);

	return PackObjectInfo(RenderableSceneBase::SceneObjectType::Rigid, mSceneRigidMeshes.size() - 1);
}

RenderableSceneObjectHandle RenderableSceneBuilderBase::PackObjectInfo(RenderableSceneBase::SceneObjectType objectType, uint32_t objectArrayIndex)
{
	return RenderableSceneObjectHandle
	{
		.Id = ((uint64_t)objectType << 56) | objectArrayIndex
	};
}
