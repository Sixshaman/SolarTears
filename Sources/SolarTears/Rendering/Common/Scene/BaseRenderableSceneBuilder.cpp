#include "BaseRenderableSceneBuilder.hpp"
#include "BaseRenderableScene.hpp"
#include <algorithm>

BaseRenderableSceneBuilder::BaseRenderableSceneBuilder(BaseRenderableScene* sceneToBuild): mSceneToBuild(sceneToBuild)
{
}

BaseRenderableSceneBuilder::~BaseRenderableSceneBuilder()
{
}

void BaseRenderableSceneBuilder::Build(const RenderableSceneDescription& sceneDescription)
{
	//After this step we'll have a flat list of meshes, and each mesh will have its submeshes sorted.
	std::vector<RenderableSceneMeshData> sceneMeshes;
	BuildMeshListWithSubmeshesSorted(sceneDescription.mSceneMeshes, sceneMeshes);


	//After this step we'll have a sorted flat list of meshes
	SortMeshesBySubmeshGeometry(sceneMeshes);


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


	//To be continued...
}

void BaseRenderableSceneBuilder::BuildMeshListWithSubmeshesSorted(const std::unordered_map<std::string, RenderableSceneMeshData>& descriptionMeshes, std::vector<RenderableSceneMeshData>& outSceneMeshes) const
{
	outSceneMeshes.clear();
	for(const auto& mesh: descriptionMeshes)
	{
		if(mesh.second.Submeshes.size() == 0)
		{
			continue;
		}

		outSceneMeshes.push_back(mesh.second);
		std::sort(outSceneMeshes.back().Submeshes.begin(), outSceneMeshes.back().Submeshes.end(), [](const RenderableSceneSubmeshData& left, const RenderableSceneSubmeshData& right)
		{
			return left.GeometryName < right.GeometryName;
		});
	}
}

void BaseRenderableSceneBuilder::SortMeshesBySubmeshGeometry(std::vector<RenderableSceneMeshData>& inoutSceneMeshes) const
{
	//First, sort the meshes by submesh count, to ensure that two meshes with geometries {"a", "b", "c"} won't be divided with a mesh with geometry {"a", "b"}
	//Sort from the highest to lowest, this is gonna be be handy later
	std::sort(inoutSceneMeshes.begin(), inoutSceneMeshes.end(), [](const RenderableSceneMeshData& left, const RenderableSceneMeshData& right)
	{
		return left.Submeshes.size() > right.Submeshes.size();
	});

	//Second, find all the ranges with equal amount of submeshes, to later sort each range
	std::vector<std::span<RenderableSceneMeshData>> meshRanges;

	size_t currentSubmeshCount = 0;
	auto currentRangeStart = inoutSceneMeshes.begin();
	for(auto meshIt = inoutSceneMeshes.begin(); meshIt != inoutSceneMeshes.end(); ++meshIt)
	{
		if(meshIt->Submeshes.size() != currentSubmeshCount)
		{
			if(std::distance(currentRangeStart, meshIt) > 1) //No point to sort single-element ranges
			{
				meshRanges.push_back({currentRangeStart, meshIt});
			}

			currentRangeStart   = meshIt;
			currentSubmeshCount = meshIt->Submeshes.size();
		}
	}

	if(std::distance(currentRangeStart, inoutSceneMeshes.end()) > 1)
	{
		meshRanges.push_back({currentRangeStart, inoutSceneMeshes.end()});
	}

	//Third, sort each range by 0th geometry name
	for(auto meshRange: meshRanges)
	{
		std::sort(meshRange.begin(), meshRange.end(), [](const RenderableSceneMeshData& left, const RenderableSceneMeshData& right)
		{
			return left.Submeshes[0].GeometryName < left.Submeshes[0].GeometryName;
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
			if(meshIt->Submeshes.size() < geometrySortIndex + 1)
			{
				currentRangeEnd = meshIt;
				break;
			}

			if(meshIt->Submeshes.size() != currentSubmeshCount || meshIt->Submeshes[geometrySortIndex].GeometryName != currentGeometryName)
			{
				if(std::distance(currentRangeStart, meshIt) > 1) //No point to sort single-element ranges
				{
					meshRanges.push_back({currentRangeStart, meshIt});
				}

				currentRangeStart   = meshIt;
				currentSubmeshCount = meshIt->Submeshes.size();
				currentGeometryName = meshIt->Submeshes[geometrySortIndex].GeometryName;
			}
		}

		if(std::distance(currentRangeStart, currentRangeEnd) > 1)
		{
			meshRanges.push_back({currentRangeStart, currentRangeEnd});
		}

		//Sort each range by ith geometry name
		for(auto meshRange: meshRanges)
		{
			std::sort(meshRange.begin(), meshRange.end(), [geometrySortIndex](const RenderableSceneMeshData& left, const RenderableSceneMeshData& right)
			{
				return left.Submeshes[geometrySortIndex].GeometryName < left.Submeshes[geometrySortIndex].GeometryName;
			});
		}

		geometrySortIndex++;
	}
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