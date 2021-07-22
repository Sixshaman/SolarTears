#include "SceneDescription.hpp"
#include "MeshComponent.hpp"

SceneDescription::SceneDescription()
{
	mSceneObjects.resize((uint32_t)Scene::SpecialSceneObjects::Count);

	SceneDescriptionObject& cameraObject = mSceneObjects[(uint32_t)Scene::SpecialSceneObjects::Camera];
	cameraObject.SetLocation(SceneObjectLocation
	{
		.Position           = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
		.Scale              = 1.0f,
		.RotationQuaternion = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f),
	});

	cameraObject.SetCameraComponent(SceneObjectCameraComponent
	{
		.VerticalFov = DirectX::XM_PIDIV2,

		.ViewportWidth  = 256,
		.ViewportHeight = 256
	});
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
	return mSceneObjects[mCameraObjectIndex];
}

void SceneDescription::SetCameraPosition(DirectX::XMVECTOR pos)
{
	DirectX::XMStoreFloat3(&mSceneObjects[mCameraObjectIndex].GetLocation().Position, pos);
}

void SceneDescription::SetCameraLook(DirectX::XMVECTOR lookNrm)
{
	DirectX::XMVECTOR defaultLook = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

#error "Not implemented"
}

void SceneDescription::SetCameraViewportParameters(float vFov, uint32_t viewportWidth, uint32_t viewportHeight)
{
	mCameraVerticalFov = vFov;

	mCameraViewportWidth = viewportWidth;
	mCameraViewportHeight = viewportHeight;
}

void SceneDescription::BuildScene(Scene* scene)
{
	scene->mSceneObjects.reserve(mSceneObjects.size());

	for(size_t i = 0; i < mSceneObjects.size(); i++)
	{
		auto renderableIt = mRenderableObjectMap.find(mSceneObjects[i].GetEntityId());
		
		RenderableSceneObjectHandle renderableHandle = INVALID_SCENE_OBJECT_HANDLE;
		if(renderableIt != mRenderableObjectMap.end())
		{
			renderableHandle = renderableIt->second;
		}

		scene->mSceneObjects.emplace_back(SceneObject
		{
			.Id       = mSceneObjects[i].GetEntityId(),
			.Location = mSceneObjects[i].GetLocation(),

			.RenderableHandle = renderableHandle
		});
	}

	//TODO: more camera control
	scene->mCamera.SetProjectionParameters(mCameraVerticalFov, (float)mCameraViewportWidth / (float)mCameraViewportHeight, 0.01f, 100.0f);

	scene->mRenderableComponentRef = mRenderableSceneRef;
}

void SceneDescription::BuildRenderableComponent(RenderableSceneBuilderBase* renderableSceneBuilder)
{
	for(size_t i = 0; i < mSceneObjects.size(); i++)
	{
		SceneObjectMeshComponent* meshComponent = mSceneObjects[i].GetMeshComponent();
		if(meshComponent != nullptr)
		{
			RenderableSceneMeshData renderableMeshData;
			renderableMeshData.Vertices.resize(meshComponent->Vertices.size());
			renderableMeshData.Indices.assign(meshComponent->Indices.begin(), meshComponent->Indices.end());
			renderableMeshData.TextureFilename = meshComponent->TextureFilename;

			if(mSceneObjects[i].IsStatic())
			{
				//Bake the location into the object info
				DirectX::XMVECTOR scale       = DirectX::XMVectorSet(mSceneObjects[i].GetLocation().Scale, mSceneObjects[i].GetLocation().Scale, mSceneObjects[i].GetLocation().Scale, 0.0f);
				DirectX::XMVECTOR translation = DirectX::XMLoadFloat3(&mSceneObjects[i].GetLocation().Position);
				DirectX::XMVECTOR rotation    = DirectX::XMLoadFloat4(&mSceneObjects[i].GetLocation().RotationQuaternion);

				DirectX::XMMATRIX meshTransform = DirectX::XMMatrixAffineTransformation(scale, DirectX::XMVectorZero(), rotation, translation);
				for(size_t ver = 0; ver < meshComponent->Vertices.size(); ver++)
				{
					DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&meshComponent->Vertices[ver].Position);
					position = DirectX::XMVector3TransformCoord(position, meshTransform);
					DirectX::XMStoreFloat3(&renderableMeshData.Vertices[ver].Position, position);

					DirectX::XMVECTOR normal = DirectX::XMLoadFloat3(&meshComponent->Vertices[ver].Normal);
					normal = DirectX::XMVector3TransformNormal(normal, meshTransform);
					DirectX::XMStoreFloat3(&renderableMeshData.Vertices[ver].Normal, normal);

					renderableMeshData.Vertices[ver].Texcoord = meshComponent->Vertices[ver].Texcoord;
				}


				RenderableSceneStaticMeshDescription meshDescription;
				meshDescription.MeshDataDescription = std::move(renderableMeshData);

				RenderableSceneObjectHandle meshHandle = renderableSceneBuilder->AddStaticSceneMesh(meshDescription);
				mRenderableObjectMap[mSceneObjects[i].GetEntityId()] = meshHandle;
			}
			else
			{
				for (size_t ver = 0; ver < meshComponent->Vertices.size(); ver++)
				{
					renderableMeshData.Vertices[ver].Position = meshComponent->Vertices[ver].Position;
					renderableMeshData.Vertices[ver].Normal   = meshComponent->Vertices[ver].Normal;
					renderableMeshData.Vertices[ver].Texcoord = meshComponent->Vertices[ver].Texcoord;
				}


				RenderableSceneRigidMeshDescription meshDescription;
				meshDescription.MeshDataDescription = std::move(renderableMeshData);
				meshDescription.InitialLocation     = mSceneObjects[i].GetLocation();

				RenderableSceneObjectHandle meshHandle = renderableSceneBuilder->AddRigidSceneMesh(meshDescription);
				mRenderableObjectMap[mSceneObjects[i].GetEntityId()] = meshHandle;
			}
		}
	}
}

void SceneDescription::BindRenderableComponent(RenderableSceneBase* renderableScene)
{
	mRenderableSceneRef = renderableScene;
}

RenderingAssetDatabase& SceneDescription::RenderingAssets()
{
	return mRenderingAssetDatabase;
}