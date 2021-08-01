#include "BaseRenderableSceneBuilder.hpp"
#include "BaseRenderableScene.hpp"
#include <algorithm>
#include <array>
#include <numeric>

BaseRenderableSceneBuilder::BaseRenderableSceneBuilder(BaseRenderableScene* sceneToBuild): mSceneToBuild(sceneToBuild)
{
}

BaseRenderableSceneBuilder::~BaseRenderableSceneBuilder()
{
}

void BaseRenderableSceneBuilder::Build(const RenderableSceneDescription& sceneDescription, const std::unordered_map<std::string_view, SceneObjectLocation>& sceneMeshInitialLocations, const SceneObjectLocation& cameraInitialLocation, const PinholeCamera& cameraInfo)
{
	//After this step we'll have a flat list of meshes, and each mesh will have its submeshes sorted.
	std::vector<NamedSceneMeshData> namedSceneMeshes;
	BuildMeshListWithSubmeshesSorted(sceneDescription.mSceneMeshes, namedSceneMeshes);

	//After this step we'll have a sorted flat list of meshes
	SortMeshesBySubmeshGeometry(namedSceneMeshes);

	//After this step we'll have a list of meshes and a list of locations
	std::vector<RenderableSceneMeshData> sceneMeshes;
	std::vector<SceneObjectLocation>     meshInitialLocations;
	ObtainLocationsFromNames(namedSceneMeshes, sceneMeshInitialLocations, sceneMeshes, meshInitialLocations);

	namedSceneMeshes.clear(); //Don't need it anymore

	//After this step we'll have instance groups formed
	std::vector<std::span<const RenderableSceneMeshData>> instanceSpans;
	std::vector<std::span<const SceneObjectLocation>>     locationSpans;
	DetectInstanceSpans(sceneMeshes, meshInitialLocations, instanceSpans, locationSpans);

	//After this step we'll have scene mesh buffers created, with info on geometry and materials for submeshes
	std::vector<uint32_t> sceneMeshToInstanceSpanIndices;
	FillMeshLists(instanceSpans, sceneMeshToInstanceSpanIndices);
	
	//After this step we'll have vertex and index buffers ready to be uploaded to GPU
	AssignSubmeshGeometries(sceneDescription.mSceneGeometries, instanceSpans, locationSpans, sceneMeshToInstanceSpanIndices);

	//After this step we'll have material data initialized and texture list prepared to be loaded
	AssignSubmeshMaterials(sceneDescription.mSceneMaterials, instanceSpans, sceneMeshToInstanceSpanIndices);

	//After this step we'll have object data initialized
	FillInitialObjectData(locationSpans, sceneMeshToInstanceSpanIndices, cameraInitialLocation, cameraInfo);

	//Finalize scene loading
	Bake();
}

void BaseRenderableSceneBuilder::BuildMeshListWithSubmeshesSorted(const std::unordered_map<std::string, RenderableSceneMeshData>& descriptionMeshes, std::vector<NamedSceneMeshData>& outNamedSceneMeshes) const
{
	outNamedSceneMeshes.clear();
	for(const auto& mesh: descriptionMeshes)
	{
		if(mesh.second.Submeshes.size() == 0)
		{
			continue;
		}

		outNamedSceneMeshes.push_back(NamedSceneMeshData
		{
			.MeshName = mesh.first,
			.MeshData = mesh.second
		});

		std::sort(outNamedSceneMeshes.back().MeshData.Submeshes.begin(), outNamedSceneMeshes.back().MeshData.Submeshes.end(), [](const RenderableSceneSubmeshData& left, const RenderableSceneSubmeshData& right)
		{
			return left.GeometryName < right.GeometryName;
		});
	}
}

void BaseRenderableSceneBuilder::SortMeshesBySubmeshGeometry(std::vector<NamedSceneMeshData>& inoutSceneMeshes) const
{
	//First, sort the meshes by submesh count, to ensure that two meshes with geometries {"a", "b", "c"} won't be divided with a mesh with geometry {"a", "b"}
	//Sort from the highest to lowest, this is gonna be be handy later
	std::sort(inoutSceneMeshes.begin(), inoutSceneMeshes.end(), [](const NamedSceneMeshData& left, const NamedSceneMeshData& right)
	{
		return left.MeshData.Submeshes.size() > right.MeshData.Submeshes.size();
	});

	//Second, find all the ranges with equal amount of submeshes, to later sort each range
	std::vector<std::span<NamedSceneMeshData>> meshRanges;

	size_t currentSubmeshCount = 0;
	auto currentRangeStart = inoutSceneMeshes.begin();
	for(auto meshIt = inoutSceneMeshes.begin(); meshIt != inoutSceneMeshes.end(); ++meshIt)
	{
		if(meshIt->MeshData.Submeshes.size() != currentSubmeshCount)
		{
			if(std::distance(currentRangeStart, meshIt) > 1) //No point to sort single-element ranges
			{
				meshRanges.push_back({currentRangeStart, meshIt});
			}

			currentRangeStart   = meshIt;
			currentSubmeshCount = meshIt->MeshData.Submeshes.size();
		}
	}

	if(std::distance(currentRangeStart, inoutSceneMeshes.end()) > 1)
	{
		meshRanges.push_back({currentRangeStart, inoutSceneMeshes.end()});
	}

	//Third, sort each range by 0th geometry name
	for(auto meshRange: meshRanges)
	{
		std::sort(meshRange.begin(), meshRange.end(), [](const NamedSceneMeshData& left, const NamedSceneMeshData& right)
		{
			return left.MeshData.Submeshes[0].GeometryName < right.MeshData.Submeshes[0].GeometryName;
		});
	}

	//Fourth, build ranges with ith geometry name present and then sort each range by ith geometry name
	size_t geometrySortIndex = 0;
	while(!meshRanges.empty())
	{
		meshRanges.clear();

		auto currentRangeStart = inoutSceneMeshes.begin();
		auto currentRangeEnd   = inoutSceneMeshes.end();

		size_t           currentSubmeshCount = 0;
		std::string_view currentGeometryName = "";
		for(auto meshIt = inoutSceneMeshes.begin(); meshIt != inoutSceneMeshes.end(); ++meshIt)
		{
			//Meshes are sorted, all further meshes won't have groupSubmeshCount submeshes
			if(meshIt->MeshData.Submeshes.size() < geometrySortIndex + 1)
			{
				currentRangeEnd = meshIt;
				break;
			}

			if(meshIt->MeshData.Submeshes.size() != currentSubmeshCount || meshIt->MeshData.Submeshes[geometrySortIndex].GeometryName != currentGeometryName)
			{
				if(std::distance(currentRangeStart, meshIt) > 1) //No point to sort single-element ranges
				{
					meshRanges.push_back({currentRangeStart, meshIt});
				}

				currentRangeStart   = meshIt;
				currentSubmeshCount = meshIt->MeshData.Submeshes.size();
				currentGeometryName = meshIt->MeshData.Submeshes[geometrySortIndex].GeometryName;
			}
		}

		if(std::distance(currentRangeStart, currentRangeEnd) > 1)
		{
			meshRanges.push_back({currentRangeStart, currentRangeEnd});
		}

		//Sort each range by ith geometry name
		for(auto meshRange: meshRanges)
		{
			std::sort(meshRange.begin(), meshRange.end(), [geometrySortIndex](const NamedSceneMeshData& left, const NamedSceneMeshData& right)
			{
				return left.MeshData.Submeshes[geometrySortIndex].GeometryName < right.MeshData.Submeshes[geometrySortIndex].GeometryName;
			});
		}

		geometrySortIndex++;
	}
}

void BaseRenderableSceneBuilder::ObtainLocationsFromNames(const std::vector<NamedSceneMeshData>& namedSceneMeshes, const std::unordered_map<std::string_view, SceneObjectLocation>& initialLocations, std::vector<RenderableSceneMeshData>& outSceneMeshes, std::vector<SceneObjectLocation>& outInitialLocations) const
{
	outSceneMeshes.clear();

	outSceneMeshes.reserve(namedSceneMeshes.size());
	outInitialLocations.reserve(namedSceneMeshes.size());

	for(const NamedSceneMeshData& namedMesh: namedSceneMeshes)
	{
		outSceneMeshes.push_back(namedMesh.MeshData);

		auto initialLocationIt = initialLocations.find(namedMesh.MeshName);
		if(initialLocationIt != initialLocations.end())
		{
			outInitialLocations.push_back(initialLocationIt->second);
		}
		else
		{
			outInitialLocations.push_back(SceneObjectLocation());
		}	
	}
}

void BaseRenderableSceneBuilder::DetectInstanceSpans(const std::vector<RenderableSceneMeshData>& sceneMeshes, const std::vector<SceneObjectLocation>& meshInitialLocations, std::vector<std::span<const RenderableSceneMeshData>>& outInstanceSpans, std::vector<std::span<const SceneObjectLocation>>& outLocationSpans) const
{
	outInstanceSpans.clear();
	outLocationSpans.clear();

	size_t meshIndex = 0;
	while(meshIndex != sceneMeshes.size())
	{
		const RenderableSceneMeshData& mesh = sceneMeshes[meshIndex];

		bool thisMeshStatic = !(mesh.MeshFlags & (uint32_t)RenderableSceneMeshFlags::NonStatic);

		size_t nextMeshIndex = meshIndex + 1;
		while(nextMeshIndex != sceneMeshes.size())
		{
			const RenderableSceneMeshData& nextMesh = sceneMeshes[nextMeshIndex];

			bool nextMeshStatic = !(nextMesh.MeshFlags & (uint32_t)RenderableSceneMeshFlags::NonStatic);
			if (!SameGeometry(mesh, nextMesh) || thisMeshStatic != nextMeshStatic)
			{
				break;
			}

			nextMeshIndex++;
		}

		outInstanceSpans.push_back({sceneMeshes.begin()          + meshIndex, sceneMeshes.begin()          + nextMeshIndex});
		outLocationSpans.push_back({meshInitialLocations.begin() + meshIndex, meshInitialLocations.begin() + nextMeshIndex});

		meshIndex = nextMeshIndex;
	}
}

void BaseRenderableSceneBuilder::FillMeshLists(std::vector<std::span<const RenderableSceneMeshData>>& instanceSpans, std::vector<uint32_t>& outSceneMeshToInstanceSpanIndices)
{
	mSceneToBuild->mSceneMeshes.clear();
	mSceneToBuild->mSceneSubmeshes.clear();

	//Do a small counting sort. First count the number of meshes, submeshes and materials for each type of mesh
	constexpr uint32_t staticIndex          = 0;
	constexpr uint32_t staticInstancedIndex = 1;
	constexpr uint32_t rigidIndex           = 2;
	constexpr uint32_t rigidInstancedIndex  = 3;

	std::array<uint32_t, 4> meshCountsPerType;
	std::array<uint32_t, 4> submeshCountsPerType;
	std::array<uint32_t, 4> objectDataCountsPerType;

	std::fill(meshCountsPerType.begin(),       meshCountsPerType.end(),       0);
	std::fill(submeshCountsPerType.begin(),    submeshCountsPerType.end(),    0);
	std::fill(objectDataCountsPerType.begin(), objectDataCountsPerType.end(), 0);

	for(std::span<const RenderableSceneMeshData> instanceSpan: instanceSpans)
	{
		const RenderableSceneMeshData& representativeMesh = instanceSpan.front();

		bool isSpanNonStatic = representativeMesh.MeshFlags & (uint32_t)RenderableSceneMeshFlags::NonStatic;
		uint32_t instanceCount = (uint32_t)instanceSpan.size();

		uint32_t submeshCount  = (uint32_t)representativeMesh.Submeshes.size();
		uint32_t materialCount = (uint32_t)representativeMesh.Submeshes.size() * (uint32_t)instanceSpan.size(); //Material data is duplicated for instances to be accessed with instance id as offset

		if(instanceCount == 1)
		{
			if(isSpanNonStatic)
			{
				meshCountsPerType[rigidIndex]       += 1;
				submeshCountsPerType[rigidIndex]    += submeshCount;
				objectDataCountsPerType[rigidIndex] += instanceCount;
			}
			else
			{
				meshCountsPerType[staticIndex]     += 1;
				submeshCountsPerType[staticIndex]  += submeshCount;

				//For static non-instanced meshes the object data is baked into geometry and is never allocated
			}
		}
		else
		{
			if(isSpanNonStatic)
			{
				meshCountsPerType[rigidInstancedIndex]       += 1;
				submeshCountsPerType[rigidInstancedIndex]    += submeshCount;
				objectDataCountsPerType[rigidInstancedIndex] += instanceCount;
			}
			else
			{
				meshCountsPerType[staticInstancedIndex]       += 1;
				submeshCountsPerType[staticInstancedIndex]    += submeshCount;
				objectDataCountsPerType[staticInstancedIndex] += instanceCount;
			}
		}
	}


	uint32_t totalMeshCount    = std::accumulate(meshCountsPerType.begin(),    meshCountsPerType.end(),    0);
	uint32_t totalSubmeshCount = std::accumulate(submeshCountsPerType.begin(), submeshCountsPerType.end(), 0);

	mSceneToBuild->mSceneMeshes.resize(totalMeshCount);
	mSceneToBuild->mSceneSubmeshes.resize(totalSubmeshCount);

	outSceneMeshToInstanceSpanIndices.resize(totalMeshCount);

	mSceneToBuild->mStaticMeshSpan.Begin = 0;
	mSceneToBuild->mStaticMeshSpan.End   = mSceneToBuild->mStaticMeshSpan.Begin + meshCountsPerType[staticIndex];

	mSceneToBuild->mStaticInstancedMeshSpan.Begin = mSceneToBuild->mStaticMeshSpan.End;
	mSceneToBuild->mStaticInstancedMeshSpan.End   = mSceneToBuild->mStaticInstancedMeshSpan.Begin + meshCountsPerType[staticInstancedIndex];

	mSceneToBuild->mRigidMeshSpan.Begin = mSceneToBuild->mStaticInstancedMeshSpan.End;
	mSceneToBuild->mRigidMeshSpan.End   = mSceneToBuild->mRigidMeshSpan.Begin + meshCountsPerType[rigidIndex] + meshCountsPerType[rigidInstancedIndex];

	uint32_t totalStaticInstancedObjectDataCount = objectDataCountsPerType[staticInstancedIndex];
	uint32_t totalRigidObjectDataCount           = objectDataCountsPerType[rigidIndex] + objectDataCountsPerType[rigidInstancedIndex];

	mInitialStaticInstancedObjectData.resize(totalStaticInstancedObjectDataCount);
	mInitialRigidObjectData.resize(totalRigidObjectDataCount);


	//Counting sort the meshes and submeshes, initializing the data
	std::array<uint32_t, 4> meshIndicesPerType;
	std::array<uint32_t, 4> submeshIndicesPerType;
	std::array<uint32_t, 4> objectDataIndicesPerType;

	//Initialize the indices with 0, c0, c0 + c1, c0 + c1 + c2
	std::exclusive_scan(meshCountsPerType.begin(),       meshCountsPerType.end(),       meshIndicesPerType.begin(),       0);
	std::exclusive_scan(submeshCountsPerType.begin(),    submeshCountsPerType.end(),    submeshIndicesPerType.begin(),    0);
	std::exclusive_scan(objectDataCountsPerType.begin(), objectDataCountsPerType.end(), objectDataIndicesPerType.begin(), 0);

	objectDataIndicesPerType[staticIndex] = (uint32_t)(-1);
	for(uint32_t instanceSpanIndex = 0; instanceSpanIndex < instanceSpans.size(); instanceSpanIndex++)
	{
		const std::span<const RenderableSceneMeshData> instanceSpan = instanceSpans[instanceSpanIndex];
		const RenderableSceneMeshData& representativeMesh = instanceSpan.front();

		bool isSpanNonStatic = representativeMesh.MeshFlags & (uint32_t)RenderableSceneMeshFlags::NonStatic;

		uint32_t instanceCount = (uint32_t)instanceSpan.size();
		uint32_t submeshCount  = (uint32_t)representativeMesh.Submeshes.size();

		uint32_t typeIndex = (uint32_t)(-1);
		if(instanceCount == 1)
		{
			if(isSpanNonStatic)
			{
				typeIndex = rigidIndex;
			}
			else
			{
				typeIndex = staticIndex;
			}
		}
		else
		{
			if(isSpanNonStatic)
			{
				typeIndex = rigidInstancedIndex;
			}
			else
			{
				typeIndex = staticInstancedIndex;
			}
		}

		mSceneToBuild->mSceneMeshes[meshIndicesPerType[typeIndex]] = BaseRenderableScene::SceneMesh
		{
			.PerObjectDataIndex    = objectDataIndicesPerType[typeIndex],
			.InstanceCount         = instanceCount,
			.FirstSubmeshIndex     = submeshIndicesPerType[typeIndex],
			.AfterLastSubmeshIndex = submeshIndicesPerType[typeIndex] + submeshCount
		};

		outSceneMeshToInstanceSpanIndices[meshIndicesPerType[typeIndex]] = instanceSpanIndex;

		meshIndicesPerType[typeIndex]    += 1;
		submeshIndicesPerType[typeIndex] += submeshCount;

		if(instanceCount > 1 || isSpanNonStatic) //Static non-instanced meshes don't have separate object data
		{
			objectDataIndicesPerType[typeIndex] += instanceCount;
		}
	}
}

void BaseRenderableSceneBuilder::AssignSubmeshGeometries(const std::unordered_map<std::string, RenderableSceneGeometryData>& descriptionGeometries, const std::vector<std::span<const RenderableSceneMeshData>>& meshInstanceSpans, const std::vector<std::span<const SceneObjectLocation>>& initialLocationSpans, const std::vector<uint32_t>& sceneMeshToInstanceSpanIndices)
{
	mVertexBufferData.clear();
	mIndexBufferData.clear();

	//For static non-instanced meshes the positional data is baked directly into geometry
	for(uint32_t meshIndex = mSceneToBuild->mStaticMeshSpan.Begin; meshIndex < mSceneToBuild->mStaticMeshSpan.End; meshIndex++)
	{
		size_t instanceSpanIndex = sceneMeshToInstanceSpanIndices[meshIndex];
		const BaseRenderableScene::SceneMesh& sceneMesh = mSceneToBuild->mSceneMeshes[meshIndex];

		const std::span<const SceneObjectLocation>     locationSpan = initialLocationSpans[instanceSpanIndex];
		const std::span<const RenderableSceneMeshData> instanceSpan = meshInstanceSpans[instanceSpanIndex];
		assert(locationSpan.size() == 1);

		uint32_t submeshCount = sceneMesh.AfterLastSubmeshIndex - sceneMesh.FirstSubmeshIndex;
		const RenderableSceneMeshData& representativeMesh = instanceSpan.front();

		const SceneObjectLocation& meshLocation = locationSpan.front();
		DirectX::XMVECTOR meshPosition = DirectX::XMLoadFloat3(&meshLocation.Position);
		DirectX::XMVECTOR meshRotation = DirectX::XMLoadFloat4(&meshLocation.RotationQuaternion);
		DirectX::XMVECTOR meshScale    = DirectX::XMVectorSet(meshLocation.Scale, meshLocation.Scale, meshLocation.Scale, 1.0f);
		
		DirectX::XMMATRIX meshMatrix = DirectX::XMMatrixAffineTransformation(meshScale, DirectX::XMVectorZero(), meshRotation, meshPosition);
		for(uint32_t submeshIndex = 0; submeshIndex < submeshCount; submeshIndex++)
		{
			const RenderableSceneGeometryData& geometryData = descriptionGeometries.at(representativeMesh.Submeshes[submeshIndex].GeometryName);

			BaseRenderableScene::SceneSubmesh& submesh = mSceneToBuild->mSceneSubmeshes[sceneMesh.FirstSubmeshIndex + submeshIndex];
			submesh.IndexCount   = (uint32_t)geometryData.Indices.size();
			submesh.FirstIndex   = (uint32_t)mIndexBufferData.size();
			submesh.VertexOffset = (uint32_t)mVertexBufferData.size();

			mVertexBufferData.reserve(mVertexBufferData.size() + geometryData.Vertices.size());
			for(size_t vertexIndex = 0; vertexIndex < geometryData.Vertices.size(); vertexIndex++)
			{
				const RenderableSceneVertex& vertex = geometryData.Vertices[vertexIndex];

				DirectX::XMVECTOR position            = DirectX::XMLoadFloat3(&vertex.Position);
				DirectX::XMVECTOR transformedPosition = DirectX::XMVector3TransformCoord(position, meshMatrix);

				DirectX::XMVECTOR normal            = DirectX::XMLoadFloat3(&vertex.Normal);
				DirectX::XMVECTOR transformedNormal = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(normal, meshMatrix));

				RenderableSceneVertex transformedVertex;
				DirectX::XMStoreFloat3(&transformedVertex.Position, transformedPosition);
				DirectX::XMStoreFloat3(&transformedVertex.Normal,   transformedNormal);
				transformedVertex.Texcoord = vertex.Texcoord;

				mVertexBufferData.push_back(std::move(transformedVertex));
			}

			mIndexBufferData.insert(mIndexBufferData.end(), geometryData.Indices.begin(), geometryData.Indices.end());
		}
	}


	struct GeometrySubmeshRange
	{
		uint32_t IndexCount;
		uint32_t FirstIndex;
		uint32_t VertexOffset;
	};

	assert(mSceneToBuild->mStaticInstancedMeshSpan.End == mSceneToBuild->mRigidMeshSpan.Begin);

	uint32_t nonBakedMeshBegin = mSceneToBuild->mStaticInstancedMeshSpan.Begin;
	uint32_t nonBakedMeshEnd   = mSceneToBuild->mRigidMeshSpan.End;

	std::unordered_map<std::string_view, GeometrySubmeshRange> geometryRanges;
	for(uint32_t meshIndex = nonBakedMeshBegin; meshIndex < nonBakedMeshEnd; meshIndex++)
	{
		size_t instanceSpanIndex = sceneMeshToInstanceSpanIndices[meshIndex];
		const BaseRenderableScene::SceneMesh& sceneMesh = mSceneToBuild->mSceneMeshes[meshIndex];

		const std::span<const RenderableSceneMeshData> instanceSpan = meshInstanceSpans[instanceSpanIndex];
		uint32_t submeshCount = sceneMesh.AfterLastSubmeshIndex - sceneMesh.FirstSubmeshIndex;

		const RenderableSceneMeshData& representativeMesh = instanceSpan.front();
		for(uint32_t submeshIndex = 0; submeshIndex < submeshCount; submeshIndex++)
		{
			BaseRenderableScene::SceneSubmesh& submesh = mSceneToBuild->mSceneSubmeshes[sceneMesh.FirstSubmeshIndex + submeshIndex];

			auto geometryRangeIt = geometryRanges.find(representativeMesh.Submeshes[submeshIndex].GeometryName);
			if(geometryRangeIt != geometryRanges.end())
			{
				submesh.IndexCount   = geometryRangeIt->second.IndexCount;
				submesh.FirstIndex   = geometryRangeIt->second.FirstIndex;
				submesh.VertexOffset = geometryRangeIt->second.VertexOffset;
			}
			else
			{
				const RenderableSceneGeometryData& geometryData = descriptionGeometries.at(representativeMesh.Submeshes[submeshIndex].GeometryName);

				GeometrySubmeshRange geometryRange = 
				{
					.IndexCount   = (uint32_t)geometryData.Indices.size(),
					.FirstIndex   = (uint32_t)mIndexBufferData.size(),
					.VertexOffset = (uint32_t)mVertexBufferData.size()
				};

				mVertexBufferData.insert(mVertexBufferData.end(), geometryData.Vertices.begin(), geometryData.Vertices.end());
				mIndexBufferData.insert(mIndexBufferData.end(),   geometryData.Indices.begin(),  geometryData.Indices.end());

				submesh.IndexCount   = geometryRange.IndexCount;
				submesh.FirstIndex   = geometryRange.FirstIndex;
				submesh.VertexOffset = geometryRange.VertexOffset;

				geometryRanges[representativeMesh.Submeshes[submeshIndex].GeometryName] = geometryRange;
			}
		}
	}
}

void BaseRenderableSceneBuilder::AssignSubmeshMaterials(const std::unordered_map<std::string, RenderableSceneMaterialData>& descriptionMaterials, const std::vector<std::span<const RenderableSceneMeshData>>& meshInstanceSpans, const std::vector<uint32_t>& sceneMeshToInstanceSpanIndices)
{
	mMaterialData.clear();
	mTexturesToLoad.clear();

	std::unordered_map<std::string_view,  uint32_t> materialIndices;
	std::unordered_map<std::wstring_view, uint32_t> textureIndices;

	for(uint32_t meshIndex = 0; meshIndex < (uint32_t)mSceneToBuild->mSceneMeshes.size(); meshIndex++)
	{
		size_t instanceSpanIndex = sceneMeshToInstanceSpanIndices[meshIndex];
		const BaseRenderableScene::SceneMesh& sceneMesh = mSceneToBuild->mSceneMeshes[meshIndex];

		uint32_t submeshCount = sceneMesh.AfterLastSubmeshIndex - sceneMesh.FirstSubmeshIndex;
		const std::span<const RenderableSceneMeshData> instanceSpan = meshInstanceSpans[instanceSpanIndex];

		for(uint32_t instanceIndex = 0; instanceIndex < sceneMesh.InstanceCount; instanceIndex++)
		{
			const RenderableSceneMeshData& meshInstance = instanceSpan[instanceIndex];
			for(uint32_t submeshIndex = 0; submeshIndex < submeshCount; submeshIndex++)
			{
				uint32_t sceneSubmeshIndex = sceneMesh.FirstSubmeshIndex + submeshIndex;

				auto materialIndexIt = materialIndices.find(meshInstance.Submeshes[submeshIndex].MaterialName);
				if(materialIndexIt != materialIndices.end())
				{
					if(sceneMesh.InstanceCount == 1)
					{
						mSceneToBuild->mSceneSubmeshes[sceneSubmeshIndex].MaterialIndex = materialIndexIt->second;
					}
					else
					{
						//The materials for instanced data are duplicated to acquire them with instance id
						mMaterialData.push_back(mMaterialData[materialIndexIt->second]);

						mSceneToBuild->mSceneSubmeshes[sceneSubmeshIndex].MaterialIndex = (uint32_t)(mMaterialData.size() - 1);
					}
				}
				else
				{
					//Add a new material
					const RenderableSceneMaterialData& materialData = descriptionMaterials.at(meshInstance.Submeshes[submeshIndex].MaterialName);
						
					RenderableSceneMaterial material;

					auto textureIndexIt = textureIndices.find(materialData.TextureFilename);
					if(textureIndexIt != textureIndices.end())
					{
						material.TextureIndex = textureIndexIt->second;
					}
					else
					{
						mTexturesToLoad.push_back(materialData.TextureFilename);

						textureIndices[materialData.TextureFilename] = (uint32_t)(mTexturesToLoad.size() - 1);
						material.TextureIndex                        = (uint32_t)(mTexturesToLoad.size() - 1);
					}

					auto normalMapIndexIt = textureIndices.find(materialData.NormalMapFilename);
					if(normalMapIndexIt != textureIndices.end())
					{
						material.NormalMapIndex = textureIndexIt->second;
					}
					else
					{
						mTexturesToLoad.push_back(materialData.NormalMapFilename);

						textureIndices[materialData.NormalMapFilename] = (uint32_t)(mTexturesToLoad.size() - 1);
						material.NormalMapIndex                        = (uint32_t)(mTexturesToLoad.size() - 1);
					}

					mMaterialData.push_back(std::move(material));

					mSceneToBuild->mSceneSubmeshes[sceneSubmeshIndex].MaterialIndex    = (uint32_t)(mMaterialData.size() - 1);
					materialIndices[meshInstance.Submeshes[submeshIndex].MaterialName] = (uint32_t)(mMaterialData.size() - 1);
				}
			}
		}
	}
}

void BaseRenderableSceneBuilder::FillInitialObjectData(const std::vector<std::span<const SceneObjectLocation>>& initialLocationSpans, const std::vector<uint32_t>& sceneMeshToInstanceSpanIndices, const SceneObjectLocation& cameraInitialLocation, const PinholeCamera& cameraInfo)
{
	//The object data views/descriptors will go for static meshes first and for rigid meshes next
	uint32_t objectDataOffset = 0;
	
	for(uint32_t meshIndex = mSceneToBuild->mStaticInstancedMeshSpan.Begin; meshIndex < mSceneToBuild->mStaticInstancedMeshSpan.End; meshIndex++)
	{
		const BaseRenderableScene::SceneMesh& sceneMesh = mSceneToBuild->mSceneMeshes[meshIndex];

		size_t instanceSpanIndex = sceneMeshToInstanceSpanIndices[meshIndex];
		const std::span<const SceneObjectLocation> locationSpan = initialLocationSpans[instanceSpanIndex];
	
		size_t perObjectIndexBegin = (size_t)sceneMesh.PerObjectDataIndex - objectDataOffset;
		for(uint32_t instanceIndex = 0; instanceIndex < sceneMesh.InstanceCount; instanceIndex++)
		{
			mInitialStaticInstancedObjectData[perObjectIndexBegin + instanceIndex] = locationSpan[instanceIndex];
		}
	}

	objectDataOffset += (uint32_t)mInitialStaticInstancedObjectData.size();

	for(uint32_t meshIndex = mSceneToBuild->mRigidMeshSpan.Begin; meshIndex < mSceneToBuild->mRigidMeshSpan.End; meshIndex++)
	{
		const BaseRenderableScene::SceneMesh& sceneMesh = mSceneToBuild->mSceneMeshes[meshIndex];

		size_t instanceSpanIndex = sceneMeshToInstanceSpanIndices[meshIndex];
		const std::span<const SceneObjectLocation> locationSpan = initialLocationSpans[instanceSpanIndex];

		size_t perObjectIndexBegin = (size_t)sceneMesh.PerObjectDataIndex - objectDataOffset;
		for(uint32_t instanceIndex = 0; instanceIndex < sceneMesh.InstanceCount; instanceIndex++)
		{
			mInitialRigidObjectData[perObjectIndexBegin + instanceIndex] = locationSpan[instanceIndex];
		}
	}

	mInitialCameraLocation   = cameraInitialLocation;
	mInitialCameraProjMatrix = cameraInfo.GetProjMatrix();
}

bool BaseRenderableSceneBuilder::SameGeometry(const RenderableSceneMeshData& left, const RenderableSceneMeshData& right) const
{
	if(left.Submeshes.size() != right.Submeshes.size())
	{
		return false;
	}

	for(size_t i = 0; i < left.Submeshes.size(); i++)
	{
		if(left.Submeshes[i].GeometryName != right.Submeshes[i].GeometryName)
		{
			return false;
		}
	}

	return true;
}