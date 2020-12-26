#include "SceneDescription.hpp"

SceneDescription::SceneDescription()
{
	mViewportWidth  = 256;
	mViewportHeight = 256;

	mCameraPosition = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	mCameraLook     = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
}

SceneDescription::~SceneDescription()
{
}

SceneDescriptionObject& SceneDescription::CreateEmptySceneObject()
{
	SceneDescriptionObject sceneDescObject(mSceneObjects.size());
	mSceneObjects.push_back(std::move(sceneDescObject));

	return mSceneObjects.back();
}

void SceneDescription::SetCameraPosition(DirectX::XMVECTOR pos)
{
	DirectX::XMStoreFloat3(&mCameraPosition, pos);
}

void SceneDescription::SetCameraLook(DirectX::XMVECTOR look)
{
	DirectX::XMStoreFloat3(&mCameraLook, look);
}

void SceneDescription::SetViewportParameters(uint32_t width, uint32_t height)
{
	mViewportWidth  = width;
	mViewportHeight = height;
}

void SceneDescription::BuildScene(Scene* scene)
{
	scene->mSceneObjects.resize(mSceneObjects.size());

	for(size_t i = 0; i < mSceneObjects.size(); i++)
	{
		Scene::SceneObject sceneObject;

		DirectX::XMStoreFloat3(&sceneObject.Location.Position,           mSceneObjects[i].GetPosition());
		DirectX::XMStoreFloat4(&sceneObject.Location.RotationQuaternion, mSceneObjects[i].GetRotation());
		sceneObject.Location.Scale = mSceneObjects[i].GetScale();

		sceneObject.Id = mSceneObjects[i].GetEntityId();
		
		sceneObject.RenderableHandle = mSceneEntityMeshes[mSceneObjects[i].GetEntityId()];

		sceneObject.DirtyFlag = true;

		scene->mSceneObjects[i] = sceneObject;
	}

	DirectX::XMVECTOR cameraPos  = DirectX::XMLoadFloat3(&mCameraPosition);
	DirectX::XMVECTOR cameraLook = DirectX::XMLoadFloat3(&mCameraLook);
	DirectX::XMVECTOR worldUp    = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	//TODO: more camera control
	scene->mCamera.SetCameraSpace(cameraPos, cameraLook, worldUp);
	scene->mCamera.SetProjectionParameters(DirectX::XM_PIDIV2, (float)mViewportWidth / (float)mViewportHeight, 0.01f, 100.0f);
}

void SceneDescription::BindRenderableComponent(RenderableSceneBuilderBase* renderableSceneBuilder)
{
	for(size_t i = 0; i < mSceneObjects.size(); i++)
	{
		SceneDescriptionObject::MeshComponent* meshComponent = mSceneObjects[i].GetMeshComponent();
		if(meshComponent != nullptr)
		{
			RenderableSceneMesh renderableMesh;
			
			renderableMesh.Vertices.resize(meshComponent->Vertices.size());
			for(size_t ver = 0; ver < meshComponent->Vertices.size(); ver++)
			{
				renderableMesh.Vertices[ver].Position = meshComponent->Vertices[ver].Position;
				renderableMesh.Vertices[ver].Normal   = meshComponent->Vertices[ver].Normal;
				renderableMesh.Vertices[ver].Texcoord = meshComponent->Vertices[ver].Texcoord;
			}

			renderableMesh.Indices.resize(meshComponent->Indices.size());
			for(size_t ind = 0; ind < meshComponent->Indices.size(); ind++)
			{
				renderableMesh.Indices[ind] = meshComponent->Indices[ind];
			}

			renderableMesh.TextureFilename = meshComponent->TextureFilename;

			RenderableSceneMeshHandle meshHandle = renderableSceneBuilder->AddSceneMesh(renderableMesh);
			mSceneEntityMeshes[mSceneObjects[i].GetEntityId()] = meshHandle;
		}
	}
}