#pragma once

#include "RenderableSceneDescription.hpp"
#include "../../../Core/DataStructures/Span.hpp"
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

	struct InstanceSpanBuckets
	{
		Span<uint32_t> StaticUniqueBucket;
		Span<uint32_t> StaticInstancedBucket;
		Span<uint32_t> RigidUniqueBucket;
		Span<uint32_t> RigidInstancedBucket;
	};

public:
	BaseRenderableSceneBuilder(BaseRenderableScene* sceneToBuild);
	~BaseRenderableSceneBuilder();

	void Build(const RenderableSceneDescription& sceneDescription, const std::unordered_map<std::string_view, SceneObjectLocation>& sceneMeshInitialLocations, std::unordered_map<std::string_view, RenderableSceneObjectHandle>& outObjectHandles);

protected:
	//Transfers the raw buffer data to GPU, loads textures, allocates per-object constant data, etc.
	virtual void Bake() = 0;

private:
	//Step 1 of filling in scene data structures
	//Creates a list of meshes sorted by submesh geometry names
	void BuildSortedMeshList(const std::unordered_map<std::string, RenderableSceneMeshData>& descriptionMeshes, std::vector<NamedSceneMeshData>& outNamedSceneMeshes) const;

	//Step 2 of filling in scene data structures
	//Groups the mesh instances together 
	void DetectInstanceSpans(const std::vector<NamedSceneMeshData>& sceneMeshes, std::vector<std::span<const NamedSceneMeshData>>& outInstanceSpans) const;

	//Step 3 of filling in scene data structures
	//Sorts the instance spans as (static meshes, static instanced meshes, rigid meshes, rigid instancedMeshes)
	void SortInstanceSpans(std::vector<std::span<const NamedSceneMeshData>>& inoutInstanceSpans, InstanceSpanBuckets* outSpanBuckets);

	//Step 4 of filling in scene data structures
	//Allocates the memory for scene mesh and submesh data
	void FillMeshLists(const std::vector<std::span<const NamedSceneMeshData>>& sortedInstanceSpans, const InstanceSpanBuckets& spanBuckets);

	//Step 5 of filling in scene data structures
	//Loads vertex and index buffer data from geometries and initializes initial positional data
	//Pre-sorting all meshes by geometry in previous steps achieves coherence
	void AssignSubmeshGeometries(const std::unordered_map<std::string, RenderableSceneGeometryData>& descriptionGeometries, const std::vector<std::span<const NamedSceneMeshData>>& sceneMeshInstanceSpans, const std::unordered_map<std::string_view, SceneObjectLocation>& sceneMeshInitialLocations);

	//Step 6 of filling in scene data structures
	//Initializes materials for scene submeshes
	void AssignSubmeshMaterials(const std::unordered_map<std::string, RenderableSceneMaterialData>& descriptionMaterials, const std::vector<std::span<const NamedSceneMeshData>>& meshInstanceSpans);

	//Step 7 of filling in scene data structures
	//Initializes initial object data
	void FillInitialObjectData(const std::vector<std::span<const NamedSceneMeshData>>& meshInstanceSpans, const std::unordered_map<std::string_view, SceneObjectLocation>& sceneMeshInitialLocations);

	//Step 8 of filling in scene data structures
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
	std::vector<std::wstring>            mTexturesToLoad;

	std::vector<SceneObjectLocation> mInitialObjectData;

	uint32_t mStaticInstancedObjectCount;
	uint32_t mRigidObjectCount;
};