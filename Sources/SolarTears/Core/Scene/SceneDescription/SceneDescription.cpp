#include "SceneDescription.hpp"
#include "MeshComponent.hpp"

SceneDescription::SceneDescription()
{
	mCameraViewportWidth  = 256;
	mCameraViewportHeight = 256;

	mCameraVerticalFov = DirectX::XM_PIDIV2;

	SceneDescriptionObject& cameraObject = mSceneObjects.emplace_back(mSceneObjects.size());
	cameraObject.SetLocation(
	{
		.Position           = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
		.Scale              = 1.0f,
		.RotationQuaternion = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f),
	});

	mCameraObjectIndex = mSceneObjects.size() - 1;
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

SceneDescriptionObject& SceneDescription::GetCameraSceneObject()
{
	return mSceneObjects[mCameraObjectIndex];
}

void SceneDescription::SetCameraPosition(DirectX::XMVECTOR pos)
{
	DirectX::XMStoreFloat3(&mSceneObjects[mCameraObjectIndex].GetLocation().Position, pos);
}

void SceneDescription::SetCameraLook(DirectX::XMVECTOR look)
{
	DirectX::XMVECTOR defaultLook    = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	DirectX::XMVECTOR normalizedLook = DirectX::XMVector3Normalize(look);

	//Find a quaternion that rotates look -> default look
	DirectX::XMVECTOR rotAxis = DirectX::XMVector3Cross(defaultLook, normalizedLook);
	if(DirectX::XMVector3NearEqual(rotAxis, DirectX::XMVectorZero(), DirectX::XMVectorSplatConstant(1, 16)))
	{
		//180 degrees rotation about an arbitrary axis
		mSceneObjects[mCameraObjectIndex].GetLocation().RotationQuaternion = DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	}
	else
	{
		//Calculate the necessary rotation quaternion q = (n * sin(theta / 2), cos(theta / 2)) that rotates by angle theta.
		//We have rotAxis = n * sin(theta). We can construct a quaternion q2 = (n * sin(theta), cos(theta)) that rotates by angle 2 * theta. 
		//Then we construct a quaternion q0 = (0, 0, 0, 1) that rotates by angle 0. Then q = normalize((q2 + q0) / 2) = normalize(q2 + q0).
		//No slerp needed since it's a perfect mid-way interpolation.
		float cosAngle = DirectX::XMVectorGetX(DirectX::XMVector3Dot(defaultLook, normalizedLook));

		DirectX::XMVECTOR unitQuaternion  = DirectX::XMQuaternionIdentity();
		DirectX::XMVECTOR twiceQuaternion = DirectX::XMVectorSetW(rotAxis, cosAngle);

		DirectX::XMVECTOR midQuaternion = DirectX::XMQuaternionNormalize(DirectX::XMVectorAdd(unitQuaternion, twiceQuaternion));
		DirectX::XMStoreFloat4(&mSceneObjects[mCameraObjectIndex].GetLocation().RotationQuaternion, midQuaternion);
	}
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

	scene->mCameraSceneObjectIndex = mCameraObjectIndex;

	for(size_t i = 0; i < mSceneObjects.size(); i++)
	{
		auto renderableIt = mSceneEntityMeshes.find(mSceneObjects[i].GetEntityId());
		
		RenderableSceneObjectHandle renderableHandle = INVALID_SCENE_OBJECT_HANDLE;
		if(renderableIt != mSceneEntityMeshes.end())
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
				mSceneEntityMeshes[mSceneObjects[i].GetEntityId()] = meshHandle;
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
				mSceneEntityMeshes[mSceneObjects[i].GetEntityId()] = meshHandle;
			}
		}
	}
}

void SceneDescription::BindRenderableComponent(RenderableSceneBase* renderableScene)
{
	mRenderableSceneRef = renderableScene;
}