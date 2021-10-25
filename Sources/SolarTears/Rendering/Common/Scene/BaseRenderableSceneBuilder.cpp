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

void BaseRenderableSceneBuilder::Build(const RenderableSceneDescription& sceneDescription, const std::unordered_map<std::string_view, SceneObjectLocation>& sceneMeshInitialLocations, std::unordered_map<std::string_view, RenderableSceneObjectHandle>& outObjectHandles)
{
	//After this step we'll have a sorted flat list of meshes
	std::vector<NamedSceneMeshData> namedSceneMeshes;
	BuildSortedMeshList(sceneDescription.mSceneMeshes, namedSceneMeshes);

	//After this step we'll have instance groups formed
	std::vector<std::span<const NamedSceneMeshData>> instanceSpans;
	DetectInstanceSpans(namedSceneMeshes, instanceSpans);

	//After this step we'll have instance groups sorted by mesh type
	SortInstanceSpans(instanceSpans);

	//After this step we'll have scene mesh buffers created
	FillMeshLists(instanceSpans);
	
	//After this step we'll have vertex and index buffers ready to be uploaded to GPU
	AssignSubmeshGeometries(sceneDescription.mSceneGeometries, instanceSpans, sceneMeshInitialLocations);

	//After this step we'll have material data initialized and texture list prepared to be loaded
	AssignSubmeshMaterials(sceneDescription.mSceneMaterials, instanceSpans);

	//After this step we'll have object data initialized
	FillInitialObjectData(instanceSpans, sceneMeshInitialLocations);

	//After this step we'll have object handles assigned
	AssignMeshHandles(instanceSpans, outObjectHandles);

	//Finalize scene loading
	Bake();
}

void BaseRenderableSceneBuilder::BuildSortedMeshList(const std::unordered_map<std::string, RenderableSceneMeshData>& descriptionMeshes, std::vector<NamedSceneMeshData>& outNamedSceneMeshes) const
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

	std::sort(outNamedSceneMeshes.begin(), outNamedSceneMeshes.end(), [](const NamedSceneMeshData& left, const NamedSceneMeshData& right)
	{
		bool leftMeshStatic  = !(left.MeshData.MeshFlags  & (uint32_t)RenderableSceneMeshFlags::NonStatic);
		bool rightMeshStatic = !(right.MeshData.MeshFlags & (uint32_t)RenderableSceneMeshFlags::NonStatic);
		if(leftMeshStatic != rightMeshStatic)
		{
			return leftMeshStatic < rightMeshStatic;
		}

		return std::lexicographical_compare(left.MeshData.Submeshes.begin(), left.MeshData.Submeshes.end(), right.MeshData.Submeshes.begin(), right.MeshData.Submeshes.end(), [](const RenderableSceneSubmeshData& submeshLeft, const RenderableSceneSubmeshData& submeshRight)
		{
			return submeshLeft.GeometryName < submeshRight.GeometryName;
		});
	});
}

void BaseRenderableSceneBuilder::DetectInstanceSpans(const std::vector<NamedSceneMeshData>& sceneMeshes, std::vector<std::span<const NamedSceneMeshData>>& outInstanceSpans) const
{
	outInstanceSpans.clear();

	auto meshIt = sceneMeshes.begin();
	while(meshIt != sceneMeshes.end())
	{
		const RenderableSceneMeshData& meshData = meshIt->MeshData;

		bool thisMeshStatic = !(meshData.MeshFlags & (uint32_t)RenderableSceneMeshFlags::NonStatic);

		auto nextMeshIt = meshIt + 1;
		while(nextMeshIt != sceneMeshes.end())
		{
			const RenderableSceneMeshData& nextMeshData = nextMeshIt->MeshData;

			bool nextMeshStatic = !(nextMeshData.MeshFlags & (uint32_t)RenderableSceneMeshFlags::NonStatic);
			if(!SameGeometry(meshData, nextMeshData) || thisMeshStatic != nextMeshStatic)
			{
				break;
			}

			++nextMeshIt;
		}

		outInstanceSpans.push_back({meshIt, nextMeshIt});
		meshIt = nextMeshIt;
	}
}

void BaseRenderableSceneBuilder::SortInstanceSpans(std::vector<std::span<const NamedSceneMeshData>>& inoutInstanceSpans)
{
	//Do a small counting sort
	uint32_t staticMeshCount          = 0;
	uint32_t staticInstancedMeshCount = 0;
	uint32_t rigidMeshCount           = 0;
	uint32_t rigidInstancedMeshCount  = 0;

	for(std::span<const NamedSceneMeshData> instanceSpan: inoutInstanceSpans)
	{
		const RenderableSceneMeshData& representativeMeshData = instanceSpan.front().MeshData;

		bool isSpanNonStatic = representativeMeshData.MeshFlags & (uint32_t)RenderableSceneMeshFlags::NonStatic;
		uint32_t instanceCount = (uint32_t)instanceSpan.size();

		if(instanceCount == 1)
		{
			if(isSpanNonStatic)
			{
				rigidMeshCount += 1;
			}
			else
			{
				staticMeshCount += 1;
			}
		}
		else
		{
			if(isSpanNonStatic)
			{
				rigidInstancedMeshCount += 1;
			}
			else
			{
				staticInstancedMeshCount += 1;
			}
		}
	}

	uint32_t totalMeshCount = staticMeshCount + staticInstancedMeshCount + rigidMeshCount + rigidInstancedMeshCount;
	std::vector<std::span<const NamedSceneMeshData>> sortedInstanceSpans(totalMeshCount);

	uint32_t staticMeshIndex          = 0;
	uint32_t staticInstancedMeshIndex = staticMeshCount;
	uint32_t rigidMeshIndex           = staticMeshCount + staticInstancedMeshCount;
	uint32_t rigidInstancedMeshIndex  = staticMeshCount + staticInstancedMeshCount + rigidMeshCount;
	for(uint32_t instanceSpanIndex = 0; instanceSpanIndex < inoutInstanceSpans.size(); instanceSpanIndex++)
	{
		const std::span<const NamedSceneMeshData> instanceSpan = inoutInstanceSpans[instanceSpanIndex];
		const RenderableSceneMeshData& representativeMeshData = instanceSpan.front().MeshData;

		bool isSpanNonStatic = representativeMeshData.MeshFlags & (uint32_t)RenderableSceneMeshFlags::NonStatic;
		uint32_t instanceCount = (uint32_t)instanceSpan.size();

		if(instanceCount == 1)
		{
			if(isSpanNonStatic)
			{
				sortedInstanceSpans[rigidMeshIndex++] = inoutInstanceSpans[instanceSpanIndex];
			}
			else
			{
				sortedInstanceSpans[staticMeshIndex++] = inoutInstanceSpans[instanceSpanIndex];
			}
		}
		else
		{
			if(isSpanNonStatic)
			{
				sortedInstanceSpans[rigidInstancedMeshIndex++] = inoutInstanceSpans[instanceSpanIndex];
			}
			else
			{
				sortedInstanceSpans[staticInstancedMeshIndex++] = inoutInstanceSpans[instanceSpanIndex];
			}
		}
	}

	std::swap(inoutInstanceSpans, sortedInstanceSpans);
}

void BaseRenderableSceneBuilder::FillMeshLists(const std::vector<std::span<const NamedSceneMeshData>>& sortedInstanceSpans)
{
	mSceneToBuild->mSceneMeshes.resize(sortedInstanceSpans.size());
	mSceneToBuild->mSceneSubmeshes.clear();

	
	mSceneToBuild->mStaticMeshSpan          = {.Begin = 0,                                            .End = (uint32_t)mSceneToBuild->mSceneMeshes.size()};
	mSceneToBuild->mStaticInstancedMeshSpan = {.Begin = (uint32_t)mSceneToBuild->mSceneMeshes.size(), .End = (uint32_t)mSceneToBuild->mSceneMeshes.size()};
	mSceneToBuild->mRigidMeshSpan           = {.Begin = (uint32_t)mSceneToBuild->mSceneMeshes.size(), .End = (uint32_t)mSceneToBuild->mSceneMeshes.size()};

	uint32_t objectDataIndex = RenderableSceneObjectHandle::UndefinedObjectBufferIndex();
	uint32_t meshIndex       = 0;
	uint32_t submeshCount    = 0;
	
	Span<uint32_t>* currentSpan = &mSceneToBuild->mStaticMeshSpan;

	uint32_t prevInstanceCount = 1;
	bool     prevSpanNonStatic = false;
	while(meshIndex < (uint32_t)sortedInstanceSpans.size()) //Go through all static meshes first
	{
		std::span<const NamedSceneMeshData> instanceSpan = sortedInstanceSpans[meshIndex];
		const RenderableSceneMeshData& representativeMeshData = instanceSpan.front().MeshData;

		uint32_t instanceCount   = (uint32_t)instanceSpan.size();
		bool     isSpanNonStatic = (representativeMeshData.MeshFlags & (uint32_t)RenderableSceneMeshFlags::NonStatic);
		
		if(instanceCount != prevInstanceCount || isSpanNonStatic != prevSpanNonStatic)
		{
			currentSpan->End = meshIndex;

			if(!isSpanNonStatic)
			{
				if(instanceCount == 1)
				{
					currentSpan     = &mSceneToBuild->mStaticMeshSpan;
					objectDataIndex = RenderableSceneObjectHandle::UndefinedObjectBufferIndex();
				}
				else
				{
					currentSpan     = &mSceneToBuild->mStaticInstancedMeshSpan;
					objectDataIndex = 0;
				}
			}
			else
			{
				currentSpan     = &mSceneToBuild->mRigidMeshSpan;
				objectDataIndex = 0;
			}

			currentSpan->Begin = meshIndex;
		}

		mSceneToBuild->mSceneMeshes[meshIndex] = BaseRenderableScene::SceneMesh
		{
			.PerObjectDataIndex    = objectDataIndex,
			.InstanceCount         = instanceCount,
			.FirstSubmeshIndex     = submeshCount,
			.AfterLastSubmeshIndex = submeshCount + (uint32_t)representativeMeshData.Submeshes.size()
		};

		if(instanceCount > 1 || isSpanNonStatic)
		{
			objectDataIndex += 1; //Static non-instanced objects don't have object data
		}

		meshIndex    += 1;
		submeshCount += (uint32_t)representativeMeshData.Submeshes.size();
	}

	currentSpan->End = meshIndex;

	mSceneToBuild->mSceneSubmeshes.resize(submeshCount);
}

void BaseRenderableSceneBuilder::AssignSubmeshGeometries(const std::unordered_map<std::string, RenderableSceneGeometryData>& descriptionGeometries, const std::vector<std::span<const NamedSceneMeshData>>& sceneMeshInstanceSpans, const std::unordered_map<std::string_view, SceneObjectLocation>& sceneMeshInitialLocations)
{
	mVertexBufferData.clear();
	mIndexBufferData.clear();

	//For static non-instanced meshes the positional data is baked directly into geometry
	for(uint32_t meshIndex = mSceneToBuild->mStaticMeshSpan.Begin; meshIndex < mSceneToBuild->mStaticMeshSpan.End; meshIndex++)
	{
		const BaseRenderableScene::SceneMesh& sceneMesh = mSceneToBuild->mSceneMeshes[meshIndex];
		std::span<const NamedSceneMeshData> instanceSpan = sceneMeshInstanceSpans[meshIndex];
		assert(instanceSpan.size() == 1);

		uint32_t submeshCount = sceneMesh.AfterLastSubmeshIndex - sceneMesh.FirstSubmeshIndex;
		const NamedSceneMeshData& representativeMesh = instanceSpan.front();

		const SceneObjectLocation& meshLocation = sceneMeshInitialLocations.at(representativeMesh.MeshName);
		DirectX::XMVECTOR meshPosition = DirectX::XMLoadFloat3(&meshLocation.Position);
		DirectX::XMVECTOR meshRotation = DirectX::XMLoadFloat4(&meshLocation.RotationQuaternion);
		DirectX::XMVECTOR meshScale    = DirectX::XMVectorSet(meshLocation.Scale, meshLocation.Scale, meshLocation.Scale, 1.0f);
		
		DirectX::XMMATRIX meshMatrix = DirectX::XMMatrixAffineTransformation(meshScale, DirectX::XMVectorZero(), meshRotation, meshPosition);
		for(uint32_t submeshIndex = 0; submeshIndex < submeshCount; submeshIndex++)
		{
			const RenderableSceneGeometryData& geometryData = descriptionGeometries.at(representativeMesh.MeshData.Submeshes[submeshIndex].GeometryName);

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
		const BaseRenderableScene::SceneMesh& sceneMesh = mSceneToBuild->mSceneMeshes[meshIndex];
		std::span<const NamedSceneMeshData> instanceSpan = sceneMeshInstanceSpans[meshIndex];

		uint32_t submeshCount = sceneMesh.AfterLastSubmeshIndex - sceneMesh.FirstSubmeshIndex;
		const RenderableSceneMeshData& representativeMeshData = instanceSpan.front().MeshData;
		for(uint32_t submeshIndex = 0; submeshIndex < submeshCount; submeshIndex++)
		{
			BaseRenderableScene::SceneSubmesh& submesh = mSceneToBuild->mSceneSubmeshes[sceneMesh.FirstSubmeshIndex + submeshIndex];

			auto geometryRangeIt = geometryRanges.find(representativeMeshData.Submeshes[submeshIndex].GeometryName);
			if(geometryRangeIt != geometryRanges.end())
			{
				submesh.IndexCount   = geometryRangeIt->second.IndexCount;
				submesh.FirstIndex   = geometryRangeIt->second.FirstIndex;
				submesh.VertexOffset = geometryRangeIt->second.VertexOffset;
			}
			else
			{
				const RenderableSceneGeometryData& geometryData = descriptionGeometries.at(representativeMeshData.Submeshes[submeshIndex].GeometryName);

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

				geometryRanges[representativeMeshData.Submeshes[submeshIndex].GeometryName] = geometryRange;
			}
		}
	}
}

void BaseRenderableSceneBuilder::AssignSubmeshMaterials(const std::unordered_map<std::string, RenderableSceneMaterialData>& descriptionMaterials, const std::vector<std::span<const NamedSceneMeshData>>& meshInstanceSpans)
{
	mMaterialData.clear();
	mTexturesToLoad.clear();

	std::unordered_map<std::string_view,  uint32_t> materialIndices;
	std::unordered_map<std::wstring_view, uint32_t> textureIndices;

	for(uint32_t meshIndex = 0; meshIndex < (uint32_t)mSceneToBuild->mSceneMeshes.size(); meshIndex++)
	{
		const std::span<const NamedSceneMeshData> instanceSpan = meshInstanceSpans[meshIndex];
		const BaseRenderableScene::SceneMesh& sceneMesh = mSceneToBuild->mSceneMeshes[meshIndex];

		uint32_t submeshCount = sceneMesh.AfterLastSubmeshIndex - sceneMesh.FirstSubmeshIndex;
		for(uint32_t submeshIndex = 0; submeshIndex < submeshCount; submeshIndex++)
		{
			uint32_t sceneSubmeshIndex = sceneMesh.FirstSubmeshIndex + submeshIndex;
			for(uint32_t instanceIndex = 0; instanceIndex < sceneMesh.InstanceCount; instanceIndex++)
			{
				const RenderableSceneMeshData& meshInstance = instanceSpan[instanceIndex].MeshData;

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
					else if (!materialData.TextureFilename.empty())
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
					else if(!materialData.NormalMapFilename.empty())
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

void BaseRenderableSceneBuilder::FillInitialObjectData(const std::vector<std::span<const NamedSceneMeshData>>& meshInstanceSpans, const std::unordered_map<std::string_view, SceneObjectLocation>& sceneMeshInitialLocations)
{
	//The object data views/descriptors will go for static meshes first and for rigid meshes next
	for(uint32_t meshIndex = mSceneToBuild->mStaticInstancedMeshSpan.Begin; meshIndex < mSceneToBuild->mStaticInstancedMeshSpan.End; meshIndex++)
	{
		const BaseRenderableScene::SceneMesh& sceneMesh = mSceneToBuild->mSceneMeshes[meshIndex];
		const std::span<const NamedSceneMeshData> instanceSpan = meshInstanceSpans[meshIndex];
	
		size_t perObjectIndexBegin = (size_t)sceneMesh.PerObjectDataIndex;
		for(uint32_t instanceIndex = 0; instanceIndex < sceneMesh.InstanceCount; instanceIndex++)
		{
			const NamedSceneMeshData& meshData = instanceSpan[instanceIndex];
			assert(sceneMesh.PerObjectDataIndex + instanceIndex == mInitialStaticInstancedObjectData.size());

			const SceneObjectLocation& meshInitialLocation = sceneMeshInitialLocations.at(meshData.MeshName);
			mInitialStaticInstancedObjectData.push_back(meshInitialLocation);
		}
	}

	for(uint32_t meshIndex = mSceneToBuild->mRigidMeshSpan.Begin; meshIndex < mSceneToBuild->mRigidMeshSpan.End; meshIndex++)
	{
		const BaseRenderableScene::SceneMesh& sceneMesh = mSceneToBuild->mSceneMeshes[meshIndex];
		const std::span<const NamedSceneMeshData> instanceSpan = meshInstanceSpans[meshIndex];

		size_t perObjectIndexBegin = (size_t)sceneMesh.PerObjectDataIndex;
		for(uint32_t instanceIndex = 0; instanceIndex < sceneMesh.InstanceCount; instanceIndex++)
		{
			const NamedSceneMeshData& meshData = instanceSpan[instanceIndex];
			assert(sceneMesh.PerObjectDataIndex + instanceIndex == mInitialRigidObjectData.size());

			const SceneObjectLocation& meshInitialLocation = sceneMeshInitialLocations.at(meshData.MeshName);
			mInitialRigidObjectData.push_back(meshInitialLocation);
		}
	}
}

void BaseRenderableSceneBuilder::AssignMeshHandles(const std::vector<std::span<const NamedSceneMeshData>>& meshInstanceSpans, std::unordered_map<std::string_view, RenderableSceneObjectHandle>& outObjectHandles)
{
	for(uint32_t meshIndex = 0; meshIndex < (uint32_t)meshInstanceSpans.size(); meshIndex++)
	{
		const BaseRenderableScene::SceneMesh& sceneMesh = mSceneToBuild->mSceneMeshes[meshIndex];
		const std::span<const NamedSceneMeshData> instanceSpan = meshInstanceSpans[meshIndex];

		for(uint32_t instanceIndex = 0; instanceIndex < (uint32_t)instanceSpan.size(); instanceIndex++)
		{
			const NamedSceneMeshData& namedMeshData = instanceSpan[instanceIndex];

			SceneObjectType objectType;
			if(namedMeshData.MeshData.MeshFlags & (uint32_t)RenderableSceneMeshFlags::NonStatic)
			{
				objectType = SceneObjectType::Rigid;
			}
			else
			{
				objectType = SceneObjectType::Static;
			}

			uint32_t objectDataIndex = sceneMesh.PerObjectDataIndex + instanceIndex;
			outObjectHandles[namedMeshData.MeshName] = RenderableSceneObjectHandle(objectDataIndex, objectType);
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