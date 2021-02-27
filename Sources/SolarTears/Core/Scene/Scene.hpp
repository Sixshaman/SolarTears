#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include "Camera.hpp"
#include "SceneObjectLocation.hpp"
#include "../../Rendering/RenderableSceneMisc.hpp"
#include "../../Input/InputSceneMisc.hpp"

class RenderableSceneBase;
class InputScene;

class Scene
{
	friend class SceneDescription;

private:
	//TODO: Cache-friendliness
	struct SceneObject
	{
		const uint64_t Id;

		RenderableSceneMeshHandle RenderableHandle;
		InputSceneControlHandle   InputHandle;

		SceneObjectLocation Location;
		bool                DirtyFlag;
	};

public:
	Scene();
	~Scene();

	void UpdateScene();

private:
	void UpdateInputComponent();
	void UpdateRenderableComponent();

private:
	std::vector<SceneObject> mSceneObjects;

	Camera mCamera;
	size_t mCameraSceneObjectIndex;

	RenderableSceneBase* mRenderableComponentRef;
	InputScene*          mInputComponentRef;
};