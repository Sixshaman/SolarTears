#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include "PinholeCamera.hpp"
#include "SceneObject.hpp"
#include "../../Rendering/Common/Scene/RenderableSceneMisc.hpp"

class Inputter;
class BaseRenderableScene;

class Scene
{
	friend class SceneDescription;

	enum class SpecialSceneObjects: uint32_t
	{
		Camera = 0,

		Count
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

	PinholeCamera mCamera;

	BaseRenderableScene*              mRenderableComponentRef;
	std::vector<ObjectDataUpdateInfo> mCurrFrameRenderableUpdates;
};