#pragma once

#include "RenderableSceneDescription.hpp"
#include <span>
#include <string_view>

class BaseRenderableScene;
class PinholeCamera;

class BaseRenderableSceneBuilder
{
	struct NamedSceneMeshData
	{
		std::string_view        MeshName;
		RenderableSceneMeshData MeshData;
	};

public:
	BaseRenderableSceneBuilder(BaseRenderableScene* sceneToBuild);
	~BaseRenderableSceneBuilder();

	void Build(const RenderableSceneDescription& sceneDescription, const std::unordered_map<std::string_view, SceneObjectLocation>& sceneMeshInitialLocations, std::unordered_map<std::string_view, RenderableSceneObjectHandle>& outObjectHandles);

protected:
	//Transfers the raw buffer data to GPU, loads textures, allocates per-object constant data, etc.
	virtual void Bake() = 0;

private:
	//Step 1 of filling scene data structures
	//Creates a list of meshes where all submeshes are sorted by geometry name
	void BuildMeshListWithSubmeshesSorted(const std::unordered_map<std::string, RenderableSceneMeshData>& descriptionMeshes, std::vector<NamedSceneMeshData>& outNamedSceneMeshes) const;

	//Step 2 of filling scene data structures
	//Sorts the meshes in such a way that instances of the same object (same geometry, maybe different materials) are next to each other
	//Assumes all submeshes in each mesh are sorted by geometries (step 1)
	void SortMeshesBySubmeshGeometry(std::vector<NamedSceneMeshData>& inoutNamedSceneMeshes) const;

	//Step 4 of filling scene data structures
	//Groups the mesh instances together 
	void DetectInstanceSpans(const std::vector<NamedSceneMeshData>& sceneMeshes, std::vector<std::span<const NamedSceneMeshData>>& outInstanceSpans) const;

	//Step 5 of filling scene data structures
	//Sorts the instance spans as (static meshes, static instanced meshes, rigid meshes)
	void SortInstanceSpans(std::vector<std::span<const NamedSceneMeshData>>& inoutInstanceSpans);

	//Step 6 of filling scene data structures
	//Allocates the memory for scene mesh and submesh data in the same order as sortedInstanceSpans
	void FillMeshLists(const std::vector<std::span<const NamedSceneMeshData>>& sortedInstanceSpans);

	//Step 7 of filling scene data structures
	//Loads vertex and index buffer data from geometries and initializes initial positional data
	//Pre-sorting all meshes are by geometry in previous steps achieves coherence
	void AssignSubmeshGeometries(const std::unordered_map<std::string, RenderableSceneGeometryData>& descriptionGeometries, const std::vector<std::span<const NamedSceneMeshData>>& sceneMeshInstanceSpans, const std::unordered_map<std::string_view, SceneObjectLocation>& sceneMeshInitialLocations);

	//Step 8 of filling scene data structures
	//Initializes materials for scene submeshes
	void AssignSubmeshMaterials(const std::unordered_map<std::string, RenderableSceneMaterialData>& descriptionMaterials, const std::vector<std::span<const NamedSceneMeshData>>& meshInstanceSpans);

	//Step 9 of filling scene data structures
	//Initializes initial object data
	void FillInitialObjectData(const std::vector<std::span<const NamedSceneMeshData>>& meshInstanceSpans, const std::unordered_map<std::string_view, SceneObjectLocation>& sceneMeshInitialLocations);

	//Step 10 of filling scene data structures
	//Builds a map of mesh name -> object handle
	void AssignMeshHandles(const std::vector<std::span<const NamedSceneMeshData>>& meshInstanceSpans, std::unordered_map<std::string_view, RenderableSceneObjectHandle>& outObjectHandles);

private:
	//Compares the geometry of two meshes. The submeshes have to be sorted by geometry name
	bool SameGeometry(const RenderableSceneMeshData& left, const RenderableSceneMeshData& right) const;

protected:
	BaseRenderableScene* mSceneToBuild;

	std::vector<RenderableSceneVertex> mVertexBufferData;
	std::vector<RenderableSceneIndex>  mIndexBufferData;

	std::vector<RenderableSceneMaterial> mMaterialData;
	std::vector<SceneObjectLocation>     mInitialStaticInstancedObjectData;
	std::vector<SceneObjectLocation>     mInitialRigidObjectData;

	std::vector<std::wstring> mTexturesToLoad;
};