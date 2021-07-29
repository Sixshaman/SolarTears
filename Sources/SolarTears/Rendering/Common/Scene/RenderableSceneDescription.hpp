#pragma once

#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include "RenderableSceneDescriptionMisc.hpp"

class RenderableSceneDescription
{
	friend class BaseRenderableSceneBuilder;

public:
	RenderableSceneDescription();
	~RenderableSceneDescription();

	void AddMaterial(const std::string& name, RenderableSceneMaterialData&& material);
	void AddGeometry(const std::string& name, RenderableSceneGeometryData&& geometry);

	void AddMesh(const std::string& name);
	void AddSubmesh(const std::string& meshName, RenderableSceneSubmeshData&& submesh);

	void MarkMeshAsNonStatic(const std::string& name);

public:
	bool IsMeshStatic(const std::string& meshName);

protected:
	std::unordered_map<std::string, RenderableSceneMeshData> mSceneMeshes;

	std::unordered_map<std::string, RenderableSceneGeometryData> mSceneGeometries;
	std::unordered_map<std::string, RenderableSceneMaterialData> mSceneMaterials;
};