#include "RenderableSceneBuilderBase.hpp"

RenderableSceneBuilderBase::RenderableSceneBuilderBase()
{
}

RenderableSceneBuilderBase::~RenderableSceneBuilderBase()
{
}

RenderableSceneMeshHandle RenderableSceneBuilderBase::AddSceneMesh(const RenderableSceneMesh& sceneMesh)
{
	mSceneMeshes.push_back(sceneMesh);
	mSceneTextures.insert(sceneMesh.TextureFilename);

	RenderableSceneMeshHandle outHandle;
	outHandle.Id = (uint32_t)(mSceneMeshes.size() - 1);

	return outHandle;
}
