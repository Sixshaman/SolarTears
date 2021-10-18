#include "SceneDescription.hpp"
#include <algorithm>

SceneDescription::SceneDescription()
{
	mSceneObjects.reserve((uint32_t)Scene::SpecialSceneObjects::Count);	
	for(uint32_t specialObjectIndex = 0; specialObjectIndex < (uint32_t)Scene::SpecialSceneObjects::Count; specialObjectIndex++)
	{
		uint64_t id = mSceneObjects.size();
		mSceneObjects.emplace_back(SceneDescriptionObject(id));
	}
	
	mSceneCameraComponent = SceneCameraComponent
	{
		.VerticalFov = DirectX::XM_PIDIV2,

		.ViewportWidth  = 256,
		.ViewportHeight = 256
	};
}

SceneDescription::~SceneDescription()
{
}

SceneDescriptionObject& SceneDescription::CreateEmptySceneObject()
{
	mSceneObjects.emplace_back(SceneDescriptionObject(mSceneObjects.size()));
	return mSceneObjects.back();
}

SceneDescriptionObject& SceneDescription::GetCameraSceneObject()
{
	return mSceneObjects[(uint32_t)Scene::SpecialSceneObjects::Camera];
}

void SceneDescription::BuildScene(Scene* scene, BaseRenderableScene* renderableComponent, const std::unordered_map<std::string_view, RenderableSceneObjectHandle>& meshHandles)
{
	scene->mCurrFrameRenderableUpdates.clear();

	//Fill scene objects
	scene->mSceneObjects.reserve(mSceneObjects.size());
	for(size_t i = 0; i < mSceneObjects.size(); i++)
	{
		RenderableSceneObjectHandle renderableHandle;

		auto renderableIt = meshHandles.find(mSceneObjects[i].GetMeshComponentName());
		if(renderableIt != meshHandles.end())
		{
			renderableHandle = renderableIt->second;

			if(!mRenderableComponentDescription.IsMeshStatic(mSceneObjects[i].GetMeshComponentName()))
			{
				scene->mCurrFrameRenderableUpdates.push_back(ObjectDataUpdateInfo
				{
					.ObjectId          = renderableHandle,
					.NewObjectLocation = mSceneObjects[i].GetLocation()
				});
			}
		}

		scene->mSceneObjects.emplace_back(SceneObject
		{
			.Id       = mSceneObjects[i].GetEntityId(),
			.Location = mSceneObjects[i].GetLocation(),

			.RenderableHandle = renderableHandle,
		});
	}

	std::sort(scene->mCurrFrameRenderableUpdates.begin(), scene->mCurrFrameRenderableUpdates.end(), [](const ObjectDataUpdateInfo& left, const ObjectDataUpdateInfo& right)
	{
		return left.ObjectId < right.ObjectId;
	});


	//TODO: more camera control
	scene->mCamera.SetProjectionParameters(mSceneCameraComponent.VerticalFov, (float)mSceneCameraComponent.ViewportWidth / (float)mSceneCameraComponent.ViewportHeight, 0.01f, 100.0f);

	//Set scene components
	scene->mRenderableComponentRef = renderableComponent;
}

RenderableSceneDescription& SceneDescription::GetRenderableComponent()
{
	return mRenderableComponentDescription;
}

void SceneDescription::GetRenderableObjectLocations(std::unordered_map<std::string_view, SceneObjectLocation>& outLocations)
{
	for(const SceneDescriptionObject& descriptionObject: mSceneObjects)
	{
		const std::string& meshComponentName = descriptionObject.GetMeshComponentName();
		if(meshComponentName.empty())
		{
			continue;
		}

		outLocations[descriptionObject.GetMeshComponentName()] = descriptionObject.GetLocation();
	}
}