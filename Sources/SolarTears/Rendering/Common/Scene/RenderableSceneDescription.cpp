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

void RenderableSceneDescription::AddMesh(const std::string& name)
{
	assert(!mSceneMeshes.contains(name));
	mSceneMeshes[name] = RenderableSceneMeshData
	{
		.Submeshes = {},
		.MeshFlags = 0
	};
}

void RenderableSceneDescription::AddSubmesh(const std::string& meshName, RenderableSceneSubmeshData&& submesh)
{
	mSceneMeshes.at(meshName).Submeshes.push_back(std::move(submesh));
}

void RenderableSceneDescription::MarkMeshAsNonStatic(const std::string& name)
{
	mSceneMeshes.at(name).MeshFlags |= (uint32_t)(RenderableSceneMeshFlags::NonStatic);
}

bool RenderableSceneDescription::IsMeshStatic(const std::string& meshName)
{
	return !(mSceneMeshes.at(meshName).MeshFlags & (uint32_t)RenderableSceneMeshFlags::NonStatic);
}