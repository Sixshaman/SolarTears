#pragma once

#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include "RenderableSceneDescriptionMisc.hpp"

class RenderableSceneDescription
{
public:
	RenderableSceneDescription();
	~RenderableSceneDescription();

	void AddMaterial(const std::string& name, RenderableSceneMaterialData&& material);
	void AddGeometry(const std::string& name, RenderableSceneGeometryData&& geometry);

	void AddStaticMesh(const std::string& name, RenderableSceneMeshData&& mesh);
	void AddRigidMesh(const std::string& name,  RenderableSceneMeshData&& mesh);

public:
	bool IsMeshStatic(const std::string& meshName);

protected:
	std::unordered_map<std::string, RenderableSceneMeshData> mSceneStaticMeshes;
	std::unordered_map<std::string, RenderableSceneMeshData> mSceneRigidMeshes;

	std::unordered_map<std::string, RenderableSceneGeometryData> mSceneGeometries;
	std::unordered_map<std::string, RenderableSceneMaterialData> mSceneMaterials;
};