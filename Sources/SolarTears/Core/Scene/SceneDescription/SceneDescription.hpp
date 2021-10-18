#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include "../Scene.hpp"
#include "SceneDescriptionObject.hpp"
#include "SpecialObjects/SceneCamera.hpp"
#include "../../../Rendering/Common/Scene/RenderableSceneDescription.hpp"

class SceneDescription
{
public:
	SceneDescription();
	~SceneDescription();

	SceneDescriptionObject& CreateEmptySceneObject();
	SceneDescriptionObject& GetCameraSceneObject();

public:
	void BuildScene(Scene* scene, BaseRenderableScene* renderableComponent, const std::unordered_map<std::string_view, RenderableSceneObjectHandle>& meshHandles);

public:
	RenderableSceneDescription& GetRenderableComponent();
	void GetRenderableObjectLocations(std::unordered_map<std::string_view, SceneObjectLocation>& outLocations);

private:
	//Scene description objects
	std::vector<SceneDescriptionObject> mSceneObjects;

	//Special object description
	SceneCameraComponent mSceneCameraComponent;

	//Scene description systems
	RenderableSceneDescription mRenderableComponentDescription;
};