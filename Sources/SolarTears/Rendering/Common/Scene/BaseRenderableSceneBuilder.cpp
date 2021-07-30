#include "BaseRenderableSceneBuilder.hpp"
#include "BaseRenderableScene.hpp"
#include <algorithm>

BaseRenderableSceneBuilder::BaseRenderableSceneBuilder(BaseRenderableScene* sceneToBuild): mSceneToBuild(sceneToBuild)
{
}

BaseRenderableSceneBuilder::~BaseRenderableSceneBuilder()
{
}

void BaseRenderableSceneBuilder::Build(const RenderableSceneDescription& sceneDescription, const std::unordered_map<std::string_view, SceneObjectLocation>& sceneMeshInitialLocations)
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


	//After this step we'll have lists of meshes of each type, with instances grouped
	std::vector<const RenderableSceneMeshData*>           staticSingleMeshes;
	std::vector<const RenderableSceneMeshData*>           rigidSingleMeshes; 
	std::vector<std::span<const RenderableSceneMeshData>> staticInstanceSpans;
	std::vector<std::span<const RenderableSceneMeshData>> rigidInstanceSpans;
	SplitMeshList(sceneMeshes, staticSingleMeshes, rigidSingleMeshes, staticInstanceSpans, rigidInstanceSpans); //Now we have 


	//After this step we'll have scene mesh buffers created, with info on geometry and materials for submeshes
	mSceneToBuild->mSceneMeshes.clear();
	mSceneToBuild->mSceneSubmeshes.clear();

	uint32_t perObjectDataOffset = 0;
	std::vector<std::string_view>            submeshGeometryNames;
	std::vector<std::string_view>            submeshMaterialNamesFlat;
	std::vector<std::span<std::string_view>> submeshMaterialNamesPerInstance;
	perObjectDataOffset += FillStaticMeshLists(staticSingleMeshes, staticInstanceSpans, perObjectDataOffset, submeshGeometryNames, submeshMaterialNamesFlat, submeshMaterialNamesPerInstance);
	perObjectDataOffset += FillRigidMeshLists(rigidSingleMeshes,   rigidInstanceSpans,  perObjectDataOffset, submeshGeometryNames, submeshMaterialNamesFlat, submeshMaterialNamesPerInstance);

	mSceneToBuild->mSceneSubmeshes.resize(submeshGeometryNames.size());
	

	


	//After this step we'll have vertex and index buffers ready to be uploaded to GPU
	AssignSubmeshGeometries(sceneDescription.mSceneGeometries, submeshGeometryNames);


	//After this step we'll have material data initialized and texture list prepared to be loaded
	AssignSubmeshMaterials(sceneDescription.mSceneMaterials, submeshMaterialNamesPerInstance);


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

void BaseRenderableSceneBuilder::ObtainLocationsFromNames(const std::vector<NamedSceneMeshData>& namedSceneMeshes, const std::unordered_map<std::string_view, SceneObjectLocation>& initialLocations, std::vector<RenderableSceneMeshData>& outSceneMeshes, std::vector<SceneObjectLocation>& outInitialLocations)
{
	outSceneMeshes.clear();

	outSceneMeshes.reserve(namedSceneMeshes.size());
	outInitialLocations.reserve(namedSceneMeshes.size());

	for(const NamedSceneMeshData& namedMesh: namedSceneMeshes)
	{
		outSceneMeshes.push_back(namedMesh.MeshData);
		outInitialLocations.push_back(initialLocations.at(namedMesh.MeshName));
	}
}

void BaseRenderableSceneBuilder::DetectInstanceSpans(std::vector<RenderableSceneMeshData>& sceneMeshes, std::vector<std::span<const RenderableSceneMeshData>>& outInstanceSpans)
{
	outInstanceSpans.clear();

	auto meshIt = sceneMeshes.begin();
	while(meshIt != sceneMeshes.end())
	{
		bool thisMeshStatic = !(meshIt->MeshFlags & (uint32_t)RenderableSceneMeshFlags::NonStatic);

		auto nextMeshIt = meshIt + 1;
		while (nextMeshIt != sceneMeshes.end())
		{
			bool nextMeshStatic = !(meshIt->MeshFlags & (uint32_t)RenderableSceneMeshFlags::NonStatic);
			if (!SameGeometry(*meshIt, *nextMeshIt) || thisMeshStatic != nextMeshStatic)
			{
				break;
			}

			++nextMeshIt;
		}

		outInstanceSpans.push_back({meshIt, nextMeshIt});
		meshIt = nextMeshIt;
	}

	//Sort instance spans by size
	std::sort(outInstanceSpans.begin(), outInstanceSpans.end(), [](std::span<const RenderableSceneMeshData> left, std::span<const RenderableSceneMeshData> right)
	{
		return left.size() < right.size();
	});

	//Sort instance spans to static/non-static
	std::stable_sort(outInstanceSpans.begin(), outInstanceSpans.end(), [](std::span<const RenderableSceneMeshData> left, std::span<const RenderableSceneMeshData> right)
	{
		return (left.front().MeshFlags & (uint32_t)RenderableSceneMeshFlags::NonStatic) < (right.front().MeshFlags & (uint32_t)RenderableSceneMeshFlags::NonStatic);
	});
}

void BaseRenderableSceneBuilder::SplitMeshList(const std::vector<RenderableSceneMeshData>& sceneMeshes, std::vector<const RenderableSceneMeshData*>& outStaticSingleMeshes, std::vector<const RenderableSceneMeshData*>& outRigidSingleMeshes, std::vector<std::span<const RenderableSceneMeshData>>& outStaticInstanceSpans, std::vector<std::span<const RenderableSceneMeshData>>& outRigidInstanceSpans) const
{
	outStaticSingleMeshes.clear();
	outRigidSingleMeshes.clear();
	outStaticInstanceSpans.clear();
	outRigidInstanceSpans.clear();

	auto meshIt = sceneMeshes.begin();
	while(meshIt != sceneMeshes.end())
	{
		bool thisMeshStatic = !(meshIt->MeshFlags & (uint32_t)RenderableSceneMeshFlags::NonStatic);

		auto nextMeshIt = meshIt + 1;
		while (nextMeshIt != sceneMeshes.end())
		{
			bool nextMeshStatic = !(meshIt->MeshFlags & (uint32_t)RenderableSceneMeshFlags::NonStatic);
			if (!SameGeometry(*meshIt, *nextMeshIt) || thisMeshStatic != nextMeshStatic)
			{
				break;
			}

			++nextMeshIt;
		}

		size_t instanceCount = std::distance(meshIt, nextMeshIt);
		if(instanceCount == 1)
		{
			const RenderableSceneMeshData* meshPtr = &(*meshIt);
			if(thisMeshStatic)
			{
				outStaticSingleMeshes.push_back(meshPtr);
			}
			else
			{
				outRigidSingleMeshes.push_back(meshPtr);
			}
		}
		else
		{
			if(thisMeshStatic)
			{
				outStaticInstanceSpans.push_back({meshIt, nextMeshIt});
			}
			else
			{
				outRigidInstanceSpans.push_back({meshIt, nextMeshIt});
			}
		}

		meshIt = nextMeshIt;
	}
}










void BaseRenderableSceneBuilder::FillMeshLists(std::vector<std::span<const RenderableSceneMeshData>>& sortedInstanceSpans, std::vector<std::string_view>& inoutSubmeshGeometryNames, std::vector<std::string_view>& inoutSubmeshMaterialNamesFlat, std::vector<std::span<std::string_view>>& inoutSubmeshMaterialNamesPerInstance)
{
	mSceneToBuild->mStaticMeshSpan.Begin = (uint32_t)mSceneToBuild->mSceneMeshes.size();

	uint32_t objectDataOffset = 0;
	auto meshSpanIt = sortedInstanceSpans.begin();

	//Go through all static non-instanced meshes first
	while(meshSpanIt != sortedInstanceSpans.end() && meshSpanIt->size() == 1 && !(meshSpanIt->front().MeshFlags & (uint32_t)RenderableSceneMeshFlags::NonStatic))
	{
		const RenderableSceneMeshData& mesh = meshSpanIt->front();

		mSceneToBuild->mSceneMeshes.push_back(BaseRenderableScene::SceneMesh
		{
			.PerObjectDataIndex    = (uint32_t)(-1), //For static non-instanced meshes this index doesn't matter
			.InstanceCount         = 1, //This is a single-instance mesh
			.FirstSubmeshIndex     = (uint32_t)mSceneToBuild->mSceneSubmeshes.size(),
			.AfterLastSubmeshIndex = (uint32_t)(mSceneToBuild->mSceneSubmeshes.size() + mesh.Submeshes.size())
		});

		for(const auto& submeshData: mesh.Submeshes)
		{
			inoutSubmeshGeometryNames.push_back(submeshData.GeometryName);

			inoutSubmeshMaterialNamesFlat.push_back(submeshData.MaterialName);
			inoutSubmeshMaterialNamesPerInstance.push_back({inoutSubmeshMaterialNamesFlat.end() - 1, inoutSubmeshMaterialNamesFlat.end()}); //Since it's a single-instance mesh, the material span is gonna be a single-element span
		}
	}

	//Then through all static instanced meshes
	while(meshSpanIt != sortedInstanceSpans.end() && meshSpanIt->size() != 1 && !(meshSpanIt->front().MeshFlags & (uint32_t)RenderableSceneMeshFlags::NonStatic))
	{
		const auto& meshSpan = *meshSpanIt;

		const auto& representativeMesh = meshSpan.front();
		const uint32_t instanceCount = (uint32_t)meshSpan.size();

		mSceneToBuild->mSceneMeshes.push_back(BaseRenderableScene::SceneMesh
		{
			.PerObjectDataIndex    = objectDataOffset,
			.InstanceCount         = instanceCount,
			.FirstSubmeshIndex     = (uint32_t)mSceneToBuild->mSceneSubmeshes.size(),
			.AfterLastSubmeshIndex = (uint32_t)(mSceneToBuild->mSceneSubmeshes.size() + representativeMesh.Submeshes.size())
		});

		for(size_t submeshIndex = 0; submeshIndex < representativeMesh.Submeshes.size(); submeshIndex++)
		{
			inoutSubmeshGeometryNames.push_back(representativeMesh.Submeshes[submeshIndex].GeometryName);
			
			for(size_t instanceIndex = 0; instanceIndex < meshSpan.size(); instanceIndex++)
			{
				inoutSubmeshMaterialNamesFlat.push_back(meshSpan[instanceIndex].Submeshes[submeshIndex].MaterialName);
			}

			inoutSubmeshMaterialNamesPerInstance.push_back({inoutSubmeshMaterialNamesFlat.end() - instanceCount, inoutSubmeshMaterialNamesFlat.end()});
		}

		objectDataOffset += instanceCount; //For instanced meshes the object data is acquired as per-object data index + instance id
	}
}

uint32_t BaseRenderableSceneBuilder::FillStaticMeshLists(const std::vector<const RenderableSceneMeshData*>& staticSingleMeshes, std::vector<std::span<const RenderableSceneMeshData>>& staticInstanceSpans, uint32_t perObjectDataOffset, std::vector<std::string_view>& inoutSubmeshGeometryNames, std::vector<std::string_view>& inoutSubmeshMaterialNamesFlat, std::vector<std::span<std::string_view>>& inoutSubmeshMaterialNamesPerInstance)
{
	uint32_t instanceCount = 0;

	mSceneToBuild->mStaticMeshSpan.Begin = (uint32_t)mSceneToBuild->mSceneMeshes.size();
	mSceneToBuild->mStaticMeshSpan.End   = (uint32_t)(mSceneToBuild->mSceneMeshes.size() + staticSingleMeshes.size());

	uint32_t staticSingleObjectDataOffset = 0; //Doesn't matter, can be any number, the position data will be baked directly into vertices
	AllocateSingleMeshesStorage(staticSingleMeshes, staticSingleObjectDataOffset, inoutSubmeshGeometryNames, inoutSubmeshMaterialNamesFlat, inoutSubmeshMaterialNamesPerInstance);


	mSceneToBuild->mStaticInstancedMeshSpan.Begin = (uint32_t)mSceneToBuild->mSceneMeshes.size();
	mSceneToBuild->mStaticInstancedMeshSpan.End   = (uint32_t)(mSceneToBuild->mSceneMeshes.size() + staticInstanceSpans.size());

	uint32_t staticInstancedObjectDataOffset = 0; //The per-object data starts with the data for static instanced objects
	instanceCount += AllocateInstancedMeshesStorage(staticInstanceSpans, staticInstancedObjectDataOffset, inoutSubmeshGeometryNames, inoutSubmeshMaterialNamesFlat, inoutSubmeshMaterialNamesPerInstance);


	return instanceCount;
}

uint32_t BaseRenderableSceneBuilder::FillRigidMeshLists(const std::vector<const RenderableSceneMeshData*>& rigidSingleMeshes, std::vector<std::span<const RenderableSceneMeshData>>& rigidInstanceSpans, uint32_t perObjectDataOffset, std::vector<std::string_view>& inoutSubmeshGeometryNames, std::vector<std::string_view>& inoutSubmeshMaterialNamesFlat, std::vector<std::span<std::string_view>>& inoutSubmeshMaterialNamesPerInstance)
{
	uint32_t instanceCount = 0;

	mSceneToBuild->mRigidMeshSpan.Begin = (uint32_t)mSceneToBuild->mSceneMeshes.size();
	mSceneToBuild->mRigidMeshSpan.End   = (uint32_t)(mSceneToBuild->mSceneMeshes.size() + rigidSingleMeshes.size() + rigidInstanceSpans.size());

	uint32_t objectDataOffset = perObjectDataOffset;
	AllocateSingleMeshesStorage(rigidSingleMeshes, objectDataOffset, inoutSubmeshGeometryNames, inoutSubmeshMaterialNamesFlat, inoutSubmeshMaterialNamesPerInstance);


	//Since single meshes only contained 1 instance per mesh, each mesh used only one object data
	objectDataOffset += (uint32_t)rigidSingleMeshes.size();
	instanceCount    += (uint32_t)rigidSingleMeshes.size();
	
	instanceCount += AllocateInstancedMeshesStorage(rigidInstanceSpans, objectDataOffset, inoutSubmeshGeometryNames, inoutSubmeshMaterialNamesFlat, inoutSubmeshMaterialNamesPerInstance);

	return instanceCount;
}

void BaseRenderableSceneBuilder::AssignSubmeshGeometries(const std::unordered_map<std::string, RenderableSceneGeometryData>& descriptionGeometries, const std::vector<std::string_view>& submeshGeometryNames)
{
	mVertexBufferData.clear();
	mIndexBufferData.clear();

	//For static non-instanced meshes the positional data is baked directly into geometry
	for(uint32_t meshIndex = mSceneToBuild->mStaticMeshSpan.Begin; meshIndex < mSceneToBuild->mStaticMeshSpan.End; meshIndex++)
	{
		const BaseRenderableScene::SceneMesh& sceneMesh = mSceneToBuild->mSceneMeshes[meshIndex];
		
		const SceneObjectLocation& meshLocation = sceneMeshLocations[meshIndex];
		DirectX::XMVECTOR meshPosition = DirectX::XMLoadFloat3(&meshLocation.Position);
		DirectX::XMVECTOR meshRotation = DirectX::XMLoadFloat4(&meshLocation.RotationQuaternion);
		DirectX::XMVECTOR meshScale    = DirectX::XMVectorSet(meshLocation.Scale, meshLocation.Scale, meshLocation.Scale, 1.0f);
		
		DirectX::XMMATRIX meshMatrix = DirectX::XMMatrixAffineTransformation(meshScale, DirectX::XMVectorZero(), meshRotation, meshPosition);
		for(uint32_t submeshIndex = sceneMesh.FirstSubmeshIndex; submeshIndex < sceneMesh.AfterLastSubmeshIndex; submeshIndex++)
		{
			//Damn! Even though C++20 has heterogeneous lookup for unordered maps, it doesn't provide default hashes for string_view
			//I don't want to boilerplate the code with definitions, so I'll just construct std::string here
			const RenderableSceneGeometryData& geometryData = descriptionGeometries.at(std::string(submeshGeometryNames[submeshIndex]));

			BaseRenderableScene::SceneSubmesh& submesh = mSceneToBuild->mSceneSubmeshes[submeshIndex];
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
	for (uint32_t meshIndex = nonBakedMeshBegin; meshIndex < nonBakedMeshEnd; meshIndex++)
	{
		const BaseRenderableScene::SceneMesh& sceneMesh = mSceneToBuild->mSceneMeshes[meshIndex];
		for(uint32_t submeshIndex = sceneMesh.FirstSubmeshIndex; submeshIndex < sceneMesh.AfterLastSubmeshIndex; submeshIndex++)
		{
			std::string_view geometryName = submeshGeometryNames[submeshIndex];
			BaseRenderableScene::SceneSubmesh& submesh = mSceneToBuild->mSceneSubmeshes[submeshIndex];

			auto geometryRangeIt = geometryRanges.find(geometryName);
			if(geometryRangeIt != geometryRanges.end())
			{
				submesh.IndexCount   = geometryRangeIt->second.IndexCount;
				submesh.FirstIndex   = geometryRangeIt->second.FirstIndex;
				submesh.VertexOffset = geometryRangeIt->second.VertexOffset;
			}
			else
			{
				const RenderableSceneGeometryData& geometryData = descriptionGeometries.at(std::string(geometryName));

				GeometrySubmeshRange geometryRange = 
				{
					.IndexCount   = (uint32_t)geometryData.Indices.size(),
					.FirstIndex   = (uint32_t)mIndexBufferData.size(),
					.VertexOffset = (uint32_t)mVertexBufferData.size()
				};

				mVertexBufferData.insert(mVertexBufferData.end(), geometryData.Vertices.begin(), geometryData.Vertices.end());
				mIndexBufferData.insert(mIndexBufferData.end(), geometryData.Indices.begin(), geometryData.Indices.end());

				submesh.IndexCount   = geometryRange.IndexCount;
				submesh.FirstIndex   = geometryRange.FirstIndex;
				submesh.VertexOffset = geometryRange.VertexOffset;

				geometryRanges[geometryName] = geometryRange;
			}
		}
	}
}

void BaseRenderableSceneBuilder::AssignSubmeshMaterials(const std::unordered_map<std::string, RenderableSceneMaterialData>& descriptionMaterials, std::vector<std::span<std::string_view>>& submeshMaterialNamesPerInstance)
{
	mMaterialData.clear();

	std::unordered_map<std::string_view,  uint32_t> materialIndices;
	std::unordered_map<std::wstring_view, uint32_t> textureIndices;

	for(uint32_t meshIndex = mSceneToBuild->mStaticMeshSpan.Begin; meshIndex < mSceneToBuild->mStaticMeshSpan.End; meshIndex++)
	{
		const BaseRenderableScene::SceneMesh& mesh = mSceneToBuild->mSceneMeshes[meshIndex];
		for(uint32_t submeshIndex = mesh.FirstSubmeshIndex; submeshIndex < mesh.AfterLastSubmeshIndex; submeshIndex++)
		{
			for(uint32_t instanceIndex = 0; instanceIndex < mesh.InstanceCount; instanceIndex++)
			{
				std::string_view materialName = submeshMaterialNamesPerInstance[submeshIndex][instanceIndex];

				auto materialIndexIt = materialIndices.find(materialName);
				if(materialIndexIt != materialIndices.end())
				{
					if(mesh.InstanceCount == 1)
					{
						mSceneToBuild->mSceneSubmeshes[submeshIndex].MaterialIndex = materialIndexIt->second;
					}
					else
					{
						//The materials for instanced data are duplicated to acquire them with instance id
						mMaterialData.push_back(mMaterialData[materialIndexIt->second]);

						mSceneToBuild->mSceneSubmeshes[submeshIndex].MaterialIndex = (uint32_t)(mMaterialData.size() - 1);
					}
				}
				else
				{
					//Add a new material
					const RenderableSceneMaterialData& materialData = descriptionMaterials.at(std::string(materialName));
						
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

					mSceneToBuild->mSceneSubmeshes[submeshIndex].MaterialIndex = (uint32_t)(mMaterialData.size() - 1);
					materialIndices[materialName]                              = (uint32_t)(mMaterialData.size() - 1);
				}
			}
		}
	}
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

void BaseRenderableSceneBuilder::AllocateSingleMeshesStorage(const std::vector<const RenderableSceneMeshData*>& singleMeshes, uint32_t objectDataOffset, std::vector<std::string_view>& inoutSubmeshGeometryNames, std::vector<std::string_view>& inoutSubmeshMaterialNamesFlat, std::vector<std::span<std::string_view>>& inoutSubmeshMaterialNamesPerInstance)
{
	for(size_t meshIndex = 0; meshIndex < singleMeshes.size(); meshIndex++)
	{
		const RenderableSceneMeshData* pMesh = singleMeshes[meshIndex];

		mSceneToBuild->mSceneMeshes.push_back(BaseRenderableScene::SceneMesh
		{
			.PerObjectDataIndex    = (uint32_t)meshIndex + objectDataOffset, //For non-instanced meshes the object data is just the index of the mesh
			.InstanceCount         = 1, //This is a single-instance mesh
			.FirstSubmeshIndex     = (uint32_t)mSceneToBuild->mSceneSubmeshes.size(),
			.AfterLastSubmeshIndex = (uint32_t)(mSceneToBuild->mSceneSubmeshes.size() + pMesh->Submeshes.size())
		});

		for(const auto& submeshData: pMesh->Submeshes)
		{
			inoutSubmeshGeometryNames.push_back(submeshData.GeometryName);

			inoutSubmeshMaterialNamesFlat.push_back(submeshData.MaterialName);
			inoutSubmeshMaterialNamesPerInstance.push_back({inoutSubmeshMaterialNamesFlat.end() - 1, inoutSubmeshMaterialNamesFlat.end()}); //Since it's a single-instance mesh, the material span is gonna be a single-element span
		}
	}
}

uint32_t BaseRenderableSceneBuilder::AllocateInstancedMeshesStorage(const std::vector<std::span<const RenderableSceneMeshData>>& meshInstanceSpans, uint32_t objectDataOffset, std::vector<std::string_view>& inoutSubmeshGeometryNames, std::vector<std::string_view>& inoutSubmeshMaterialNamesFlat, std::vector<std::span<std::string_view>>& inoutSubmeshMaterialNamesPerInstance)
{
	uint32_t totalInstanceCount = 0;

	for(size_t spanIndex = 0; spanIndex < meshInstanceSpans.size(); spanIndex++)
	{
		const auto  meshSpan           = meshInstanceSpans[spanIndex];
		const auto& representativeMesh = meshSpan[0];

		const uint32_t instanceCount = (uint32_t)meshSpan.size();

		mSceneToBuild->mSceneMeshes.push_back(BaseRenderableScene::SceneMesh
		{
			.PerObjectDataIndex    = objectDataOffset + totalInstanceCount, //For instanced meshes the object data is acquired as instance id + per-object data index
			.InstanceCount         = instanceCount,
			.FirstSubmeshIndex     = (uint32_t)mSceneToBuild->mSceneSubmeshes.size(),
			.AfterLastSubmeshIndex = (uint32_t)(mSceneToBuild->mSceneSubmeshes.size() + representativeMesh.Submeshes.size())
		});

		for(size_t submeshIndex = 0; submeshIndex < representativeMesh.Submeshes.size(); submeshIndex++)
		{
			inoutSubmeshGeometryNames.push_back(representativeMesh.Submeshes[submeshIndex].GeometryName);
			
			for(size_t instanceIndex = 0; instanceIndex < meshSpan.size(); instanceIndex++)
			{
				inoutSubmeshMaterialNamesFlat.push_back(meshSpan[instanceIndex].Submeshes[submeshIndex].MaterialName);
			}

			inoutSubmeshMaterialNamesPerInstance.push_back({inoutSubmeshMaterialNamesFlat.end() - meshSpan.size(), inoutSubmeshMaterialNamesFlat.end()});
		}

		totalInstanceCount += instanceCount;
	}

	return totalInstanceCount;
}