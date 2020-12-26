#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include "Camera.hpp"
#include "../../Rendering/RenderableSceneMisc.hpp"

class RenderableSceneBase;

class Scene
{
	friend class SceneDescription;

public:
	struct SceneObjectLocation
	{
		DirectX::XMFLOAT3 Position;
		float             Scale;
		DirectX::XMFLOAT4 RotationQuaternion;
	};

private:
	//TODO: Cache-friendliness
	struct SceneObject
	{
		uint64_t                  Id;
		RenderableSceneMeshHandle RenderableHandle;

		SceneObjectLocation Location;
		bool                DirtyFlag;
	};

public:
	Scene();
	~Scene();

	//Camera functions
	void MoveCameraForward(float distance);
	void MoveCameraSideways(float distance);

	void RotateCamera(float horizontalAngle, float verticalAngle);

	void SetCameraAspectRatio(uint32_t width, uint32_t height);

public:
	void UpdateRenderableScene(RenderableSceneBase* renderableScene);

private:
	std::vector<SceneObject> mSceneObjects;

	Camera mCamera;
};