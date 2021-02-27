#include "Scene.hpp"
#include "../../Rendering/RenderableSceneBase.hpp"
#include "../../Input/InputScene.hpp"

Scene::Scene()
{
}

Scene::~Scene()
{
}

void Scene::UpdateScene()
{
	UpdateInputComponent();
	UpdateRenderableComponent();
}

void Scene::UpdateInputComponent()
{
	//TODO: keep the list of objects that can be controlled
	for (size_t i = 0; i < mSceneObjects.size(); i++)
	{
		if(mSceneObjects[i].InputHandle != INVALID_SCENE_CONTROL_HANDLE && mInputComponentRef->IsLocationChanged(mSceneObjects[i].InputHandle))
		{
			mSceneObjects[i].Location = mInputComponentRef->GetUpdatedLocation(mSceneObjects[i].InputHandle);
			mSceneObjects[i].DirtyFlag = true;
		}
	}
}

void Scene::UpdateRenderableComponent()
{
	//TODO: keep only the list of changed objects (no dirty flag)
	for(size_t i = 0; i < mSceneObjects.size(); i++)
	{
		if(mSceneObjects[i].RenderableHandle != INVALID_SCENE_MESH_HANDLE && mSceneObjects[i].DirtyFlag)
		{
			mRenderableComponentRef->UpdateSceneMeshData(mSceneObjects[i].RenderableHandle, mSceneObjects[i].Location);
			mSceneObjects[i].DirtyFlag = false;
		}
	}

	//The matrix from camera space to world space
	DirectX::XMVECTOR cameraPosition       = DirectX::XMLoadFloat3(&mSceneObjects[mCameraSceneObjectIndex].Location.Position);
	DirectX::XMVECTOR cameraRotation       = DirectX::XMLoadFloat4(&mSceneObjects[mCameraSceneObjectIndex].Location.RotationQuaternion);
	DirectX::XMVECTOR cameraRotationOrigin = DirectX::XMVectorZero();
	DirectX::XMVECTOR cameraScale          = DirectX::XMVectorSplatOne();
	DirectX::XMMATRIX invViewMatrix = DirectX::XMMatrixAffineTransformation(cameraScale, cameraRotationOrigin, cameraRotation, cameraPosition);

	mCamera.RecalcProjMatrix();
	mRenderableComponentRef->UpdateSceneCameraData(DirectX::XMMatrixInverse(nullptr, invViewMatrix), mCamera.GetProjMatrix());

	mRenderableComponentRef->FinalizeSceneUpdating();
}