#pragma once

#include "RenderableSceneDescription.hpp"
#include <span>
#include <string_view>

class BaseRenderableScene;

class BaseRenderableSceneBuilder
{
public:
	BaseRenderableSceneBuilder(BaseRenderableScene* sceneToBuild);
	~BaseRenderableSceneBuilder();

	void Build(const RenderableSceneDescription& sceneDescription);

private:
	//Step 1 of filling scene data structures
	//Creates a list of meshes where all submeshes are sorted by geometry name
	void BuildMeshListWithSubmeshesSorted(const std::unordered_map<std::string, RenderableSceneMeshData>& descriptionMeshes, std::vector<RenderableSceneMeshData>& outSceneMeshes) const;

	//Step 2 of filling scene data structures
	//Sorts the meshes in such a way that instances of the same object (same geometry, maybe different materials) are next to each other
	//Assumes all submeshes in each mesh are sorted by geometries (step 1)
	void SortMeshesBySubmeshGeometry(std::vector<RenderableSceneMeshData>& inoutSceneMeshes) const;

	//Step 3 of filling scene data structures
	//Splits a single mesh list onto 2 groups: static and rigid, with instance spans marked
	void SplitMeshList(const std::vector<RenderableSceneMeshData>& sceneMeshes, std::vector<const RenderableSceneMeshData*>& outStaticSingleMeshes, std::vector<const RenderableSceneMeshData*>& outRigidSingleMeshes, std::vector<std::span<const RenderableSceneMeshData>>& outStaticInstanceSpans, std::vector<std::span<const RenderableSceneMeshData>>& outRigidInstanceSpans) const;

	//Step 4 of filling scene data structures
	//Allocates the actual submesh and mesh lists. Both functions returns the total instance count written
	uint32_t FillStaticMeshLists(const std::vector<const RenderableSceneMeshData*>& staticSingleMeshes, std::vector<std::span<const RenderableSceneMeshData>>& staticInstanceSpans, uint32_t perObjectDataOffset, std::vector<std::string_view>& inoutSubmeshGeometryNames, std::vector<std::string_view>& inoutSubmeshMaterialNamesFlat, std::vector<std::span<std::string_view>>& inoutSubmeshMaterialNamesPerInstance);
	uint32_t FillRigidMeshLists(const std::vector<const RenderableSceneMeshData*>& rigidSingleMeshes, std::vector<std::span<const RenderableSceneMeshData>>& rigidInstanceSpans, uint32_t perObjectDataOffset, std::vector<std::string_view>& inoutSubmeshGeometryNames, std::vector<std::string_view>& inoutSubmeshMaterialNamesFlat, std::vector<std::span<std::string_view>>& inoutSubmeshMaterialNamesPerInstance);

	//Step 5 of filling scene data structures
	//Loads vertex and index buffer data from geometries
	//Pre-sorting all meshes are by geometry in previous steps achieves coherence
	void AssignSubmeshGeometries(const std::unordered_map<std::string, RenderableSceneGeometryData>& descriptionGeometries, const std::vector<std::string_view>& submeshGeometryNames);

	//Step 6 of filling scene data structures
	void AssignSubmeshMaterials(const std::unordered_map<std::string, RenderableSceneMaterialData>& descriptionMaterials, std::vector<std::span<std::string_view>>& submeshMaterialNamesPerInstance);

private:
	//Compares the geometry of two meshes. The submeshes have to be sorted by geometry name
	bool SameGeometry(const RenderableSceneMeshData& left, const RenderableSceneMeshData& right) const;

	//Allocates scene storage for single meshes
	void AllocateSingleMeshesStorage(const std::vector<const RenderableSceneMeshData*>& singleMeshes, uint32_t objectDataOffset, std::vector<std::string_view>& inoutSubmeshGeometryNames, std::vector<std::string_view>& inoutSubmeshMaterialNamesFlat, std::vector<std::span<std::string_view>>& inoutSubmeshMaterialNamesPerInstance);

	//Allocates scene storage for instanced meshes. Returns the total instance count
	uint32_t AllocateInstancedMeshesStorage(const std::vector<std::span<const RenderableSceneMeshData>>& meshInstanceSpans, uint32_t objectDataOffset, std::vector<std::string_view>& inoutSubmeshGeometryNames, std::vector<std::string_view>& inoutSubmeshMaterialNamesFlat, std::vector<std::span<std::string_view>>& inoutSubmeshMaterialNamesPerInstance);

private:
	BaseRenderableScene* mSceneToBuild;

	std::vector<RenderableSceneVertex> mVertexBufferData;
	std::vector<RenderableSceneIndex>  mIndexBufferData;

	std::vector<RenderableSceneMaterial> mMaterialData;

	std::vector<std::wstring> mTexturesToLoad;
};