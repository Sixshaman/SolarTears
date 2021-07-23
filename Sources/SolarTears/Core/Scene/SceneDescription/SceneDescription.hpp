#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include "CameraComponent.hpp"
#include "SceneDescriptionObject.hpp"
#include "AssetDatabase/RenderingAssetDatabase.hpp"
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
	void BindRenderableComponent(RenderableSceneBase* renderableScene);
	void BuildRenderableComponent(RenderableSceneBuilderBase* renderableSceneBuilder);

	RenderingAssetDatabase& RenderingAssets();

private:
	RenderingAssetDatabase mRenderingAssetDatabase;

	std::vector<SceneDescriptionObject> mSceneObjects;

	RenderableSceneBase*                                      mRenderableSceneRef;
	std::unordered_map<uint64_t, RenderableSceneObjectHandle> mRenderableObjectMap;
};