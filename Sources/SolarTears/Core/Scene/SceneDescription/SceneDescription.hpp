#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include "SceneDescriptionObject.hpp"
#include "../../../Rendering/Common/Scene/RenderableSceneBuilderBase.hpp"

class SceneDescription
{
public:
	SceneDescription();
	~SceneDescription();

	SceneDescriptionObject& CreateEmptySceneObject();

	SceneDescriptionObject& GetCameraSceneObject();

public:
	void BuildScene(Scene* scene);

public:
	void SetCameraPosition(DirectX::XMVECTOR pos);
	void SetCameraLook(DirectX::XMVECTOR look);

	void SetCameraViewportParameters(float vFov, uint32_t viewportWidth, uint32_t viewportHeight);
	
	void BuildRenderableComponent(RenderableSceneBuilderBase* renderableSceneBuilder);
	void BindRenderableComponent(RenderableSceneBase* renderableScene);

private:
	std::vector<SceneDescriptionObject> mSceneObjects;

	std::unordered_map<uint64_t, RenderableSceneMeshHandle> mSceneEntityMeshes;

	RenderableSceneBase* mRenderableSceneRef;

	size_t mCameraObjectIndex;

	float    mCameraVerticalFov;
	uint32_t mCameraViewportWidth;
	uint32_t mCameraViewportHeight;
};