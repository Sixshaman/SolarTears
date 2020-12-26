#include "Scene.hpp"
#include "../../Rendering/RenderableSceneBase.hpp"

Scene::Scene()
{
}

Scene::~Scene()
{
}

void Scene::MoveCameraForward(float distance)
{
	mCamera.WalkForward(distance);
}

void Scene::MoveCameraSideways(float distance)
{
	mCamera.WalkSideways(distance);
}

void Scene::RotateCamera(float horizontalAngle, float verticalAngle)
{
	mCamera.Pitch(verticalAngle);
	mCamera.Yaw(horizontalAngle);
}

void Scene::SetCameraAspectRatio(uint32_t width, uint32_t height)
{
	//TODO: more camera control
	mCamera.SetProjectionParameters(mCamera.GetFovY(), (float)width / (float)height, mCamera.GetNearZ(), mCamera.GetFarZ());
}

void Scene::UpdateRenderableScene(RenderableSceneBase* renderableScene)
{
	//TODO: keep only the list of changed objects (no dirty flag)
	for(size_t i = 0; i < mSceneObjects.size(); i++)
	{
		if(mSceneObjects[i].RenderableHandle != INVALID_SCENE_MESH_HANDLE && mSceneObjects[i].DirtyFlag)
		{
			renderableScene->UpdateSceneMeshData(mSceneObjects[i].RenderableHandle, mSceneObjects[i].Location);
			mSceneObjects[i].DirtyFlag = false;
		}
	}

	//TODO: only recalculate if needed
	mCamera.RecalcViewMatrix();
	mCamera.RecalcProjMatrix();
	renderableScene->UpdateSceneCameraData(mCamera.GetViewMatrix(), mCamera.GetProjMatrix());
}