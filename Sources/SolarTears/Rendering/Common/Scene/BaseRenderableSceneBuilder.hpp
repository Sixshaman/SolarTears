#pragma once

#include "RenderableSceneDescription.hpp"

class BaseRenderableSceneBuilder
{
public:
	BaseRenderableSceneBuilder();
	~BaseRenderableSceneBuilder();

	void Build(const RenderableSceneDescription& sceneDescription);

private:
	//Step 1 of gathering the necessary data
	//Creates a list of meshes where all submeshes are sorted by geometry name
	void BuildMeshListWithSubmeshesSorted(const std::unordered_map<std::string, RenderableSceneMeshData>& descriptionMeshes, std::vector<RenderableSceneMeshData>& outSceneMeshes);

	//Step 2 of gathering the necessary data
	//Sorts the meshes in such a way that instances of the same object (same geometry, maybe different materials) are next to each other
	//Assumes all submeshes in each mesh are sorted by geometries (step 1)
	void SortMeshesBySubmeshGeometry(std::vector<RenderableSceneMeshData>& inoutSceneMeshes);
};