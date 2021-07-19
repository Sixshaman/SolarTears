#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include "Camera.hpp"
#include "../../Rendering/Common/Scene/RenderableSceneMisc.hpp"

class RenderableSceneBase;
class Inputter;

class Scene
{
	friend class SceneDescription;

private:
	struct SceneObject
	{
		const uint64_t Id;

		RenderableSceneMeshHandle RenderableHandle;
	};

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
};