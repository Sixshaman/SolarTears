#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include "Camera.hpp"
#include "SceneObject.hpp"
#include "../../Rendering/Common/Scene/RenderableSceneBase.hpp"

class Inputter;

class Scene
{
	friend class SceneDescription;

public:
	Scene();
	~Scene();

	void ProcessControls(Inputter* inputter, float dt);
	void UpdateScene();

private:
	void UpdateRenderableComponent();

private:
	std::vector<SceneObject> mSceneObjects;

	Camera mCamera;
	size_t mCameraSceneObjectIndex;

	RenderableSceneBase* mRenderableComponentRef;

	std::vector<ObjectDataUpdateInfo> mCurrFrameRenderableUpdates;
};