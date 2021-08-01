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

	void Build(const RenderableSceneDescription& sceneDescription, const std::unordered_map<std::string_view, SceneObjectLocation>& sceneMeshInitialLocations, const SceneObjectLocation& cameraInitialLocation, const PinholeCamera& cameraInfo);

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

	//Step 3 of filling scene data structures
	//Creates a list of mesh infos and a list of mesh initial locations
	void ObtainLocationsFromNames(const std::vector<NamedSceneMeshData>& namedSceneMeshes, const std::unordered_map<std::string_view, SceneObjectLocation>& initialLocations, std::vector<RenderableSceneMeshData>& outSceneMeshes, std::vector<SceneObjectLocation>& outInitialLocations) const;

	//Step 4 of filling scene data structures
	//Groups the mesh instances together 
	void DetectInstanceSpans(const std::vector<RenderableSceneMeshData>& sceneMeshes, const std::vector<SceneObjectLocation>& meshInitialLocations, std::vector<std::span<const RenderableSceneMeshData>>& outInstanceSpans, std::vector<std::span<const SceneObjectLocation>>& outLocationSpans) const;

	//Step 5 of filling scene data structures
	//Allocates the memory for scene mesh and submesh data in sorted order (static meshes, static instanced meshes, rigid meshes)
	void FillMeshLists(std::vector<std::span<const RenderableSceneMeshData>>& instanceSpans, std::vector<uint32_t>& outSceneMeshToInstanceSpanIndices);

	//Step 6 of filling scene data structures
	//Loads vertex and index buffer data from geometries and initializes initial positional data
	//Pre-sorting all meshes are by geometry in previous steps achieves coherence
	void AssignSubmeshGeometries(const std::unordered_map<std::string, RenderableSceneGeometryData>& descriptionGeometries, const std::vector<std::span<const RenderableSceneMeshData>>& meshInstanceSpans, const std::vector<std::span<const SceneObjectLocation>>& initialLocationSpans, const std::vector<uint32_t>& sceneMeshToInstanceSpanIndices);

	//Step 7 of filling scene data structures
	//Initializes materials for scene submeshes
	void AssignSubmeshMaterials(const std::unordered_map<std::string, RenderableSceneMaterialData>& descriptionMaterials, const std::vector<std::span<const RenderableSceneMeshData>>& meshInstanceSpans, const std::vector<uint32_t>& sceneMeshToInstanceSpanIndices);

	//Step 8 of filling scene data structures
	//Initializes initial object data
	void FillInitialObjectData(const std::vector<std::span<const SceneObjectLocation>>& initialLocationSpans, const std::vector<uint32_t>& sceneMeshToInstanceSpanIndices, const SceneObjectLocation& cameraInitialLocation, const PinholeCamera& cameraInfo);

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

	SceneObjectLocation mInitialCameraLocation;
	DirectX::XMFLOAT4X4 mInitialCameraProjMatrix;

	std::vector<std::wstring> mTexturesToLoad;
};