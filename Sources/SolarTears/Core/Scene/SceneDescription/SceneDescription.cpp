#include "SceneDescription.hpp"

SceneDescription::SceneDescription()
{
	mCameraViewportWidth  = 256;
	mCameraViewportHeight = 256;

	mCameraVerticalFov = DirectX::XM_PIDIV2;

	SceneDescriptionObject& cameraObject = mSceneObjects.emplace_back(mSceneObjects.size());
	cameraObject.SetPosition(DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f));
	cameraObject.SetRotation(DirectX::XMQuaternionIdentity());
	cameraObject.SetScale(1.0f);

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
	mSceneObjects[mCameraObjectIndex].SetPosition(pos);
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
		mSceneObjects[mCameraObjectIndex].SetRotation(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
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
		mSceneObjects[mCameraObjectIndex].SetRotation(midQuaternion);
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
		Scene::SceneObject sceneObject = {.Id = mSceneObjects[i].GetEntityId()};

		DirectX::XMStoreFloat3(&sceneObject.Location.Position,           mSceneObjects[i].GetPosition());
		DirectX::XMStoreFloat4(&sceneObject.Location.RotationQuaternion, mSceneObjects[i].GetRotation());
		sceneObject.Location.Scale = mSceneObjects[i].GetScale();
		
		sceneObject.RenderableHandle = mSceneEntityMeshes[mSceneObjects[i].GetEntityId()];

		sceneObject.DirtyFlag = true;

		scene->mSceneObjects.emplace_back(sceneObject);
	}

	//TODO: more camera control
	scene->mCamera.SetProjectionParameters(mCameraVerticalFov, (float)mCameraViewportWidth / (float)mCameraViewportHeight, 0.01f, 100.0f);

	scene->mRenderableComponentRef = mRenderableSceneRef;
}

void SceneDescription::BuildRenderableComponent(RenderableSceneBuilderBase* renderableSceneBuilder)
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

void SceneDescription::BindRenderableComponent(RenderableSceneBase* renderableScene)
{
	mRenderableSceneRef = renderableScene;
}