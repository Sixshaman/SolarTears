#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include "SceneDescriptionObject.hpp"
#include "../../Rendering/RenderableSceneBuilderBase.hpp"

class SceneDescription
{
public:
	SceneDescription();
	~SceneDescription();

	SceneDescriptionObject& CreateEmptySceneObject();

	void SetCameraPosition(DirectX::XMVECTOR pos);
	void SetCameraLook(DirectX::XMVECTOR look);

	void SetViewportParameters(uint32_t width, uint32_t height);

public:
	void BuildScene(Scene* scene);

public:
	void BuildRenderableComponent(RenderableSceneBuilderBase* renderableSceneBuilder);
	void BindRenderableComponent(RenderableSceneBase* renderableScene);

private:
	std::vector<SceneDescriptionObject> mSceneObjects;

	DirectX::XMFLOAT3 mCameraPosition;
	DirectX::XMFLOAT3 mCameraLook;

	uint32_t mViewportWidth;
	uint32_t mViewportHeight;

	std::unordered_map<uint64_t, RenderableSceneMeshHandle> mSceneEntityMeshes;

	RenderableSceneBase* mRenderableSceneRef;
};