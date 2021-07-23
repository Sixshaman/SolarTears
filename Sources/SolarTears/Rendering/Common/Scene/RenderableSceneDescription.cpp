#include "RenderableSceneDescription.hpp"

RenderableSceneDescription::RenderableSceneDescription()
{
}

RenderableSceneDescription::~RenderableSceneDescription()
{
}

void RenderableSceneDescription::AddMaterial(const std::string& name, RenderableSceneMaterialData&& material)
{
	assert(!mSceneMaterials.contains(name));
	mSceneMaterials[name] = std::move(material);
}

void RenderableSceneDescription::AddGeometry(const std::string& name, RenderableSceneGeometryData&& geometry)
{
	assert(!mSceneGeometries.contains(name));
	mSceneGeometries[name] = std::move(geometry);
}

void RenderableSceneDescription::AddStaticMesh(const std::string& name, RenderableSceneMeshData&& mesh)
{
	assert(!mSceneStaticMeshes.contains(name));
	assert(!mSceneRigidMeshes.contains(name));
	assert(mSceneMaterials.contains(mesh.MaterialName));
	assert(mSceneGeometries.contains(mesh.GeometryName));

	mSceneStaticMeshes[name] = std::move(mesh);
}

void RenderableSceneDescription::AddRigidMesh(const std::string& name, RenderableSceneMeshData&& mesh)
{
	assert(!mSceneStaticMeshes.contains(name));
	assert(!mSceneRigidMeshes.contains(name));
	assert(mSceneMaterials.contains(mesh.MaterialName));
	assert(mSceneGeometries.contains(mesh.GeometryName));

	mSceneRigidMeshes[name] = std::move(mesh);
}

bool RenderableSceneDescription::IsMeshStatic(const std::string& meshName)
{
	return mSceneStaticMeshes.contains(meshName);
}