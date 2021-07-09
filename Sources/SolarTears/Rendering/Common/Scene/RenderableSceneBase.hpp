#pragma once

#include <DirectXMath.h>
#include "RenderableSceneMisc.hpp"
#include "../../../Core/Scene/Scene.hpp"
#include <span>

struct ObjectDataUpdateInfo
{
	RenderableSceneMeshHandle ObjectId;
	SceneObjectLocation       ObjectLocation;
};

struct alignas(DirectX::XMMATRIX) FrameDataUpdateInfo
{
	DirectX::XMMATRIX ViewMatrix;
	DirectX::XMMATRIX ProjMatrix;
};

class RenderableSceneBase
{
protected:
	struct PerObjectData
	{
		DirectX::XMFLOAT4X4 WorldMatrix;
	};

	struct PerFrameData
	{
		DirectX::XMFLOAT4X4 ViewProjMatrix;
	};

	struct SceneSubobject
	{
		uint32_t IndexCount;
		uint32_t FirstIndex;
		int32_t  VertexOffset;
		uint32_t MaterialIndex;
	};

	struct SceneMaterial
	{
		uint32_t TextureIndex;
		uint32_t NormalMapIndex;
	};

	struct SceneObject
	{
		uint32_t PerObjectDataIndex;
		uint32_t FirstSubobjectIndex;
		uint32_t AfterLastSubobjectIndex;
	};

public:
	RenderableSceneBase(uint32_t maxDirtyFrames);
	~RenderableSceneBase();

	virtual void UpdateSceneObjects(const FrameDataUpdateInfo& frameUpdate, const std::span<ObjectDataUpdateInfo> objectUpdates) = 0;

protected:
	PerObjectData PackObjectData(const SceneObjectLocation& sceneObjectLocation)  const;
	PerFrameData  PackFrameData(DirectX::FXMMATRIX View, DirectX::FXMMATRIX Proj) const;

protected:
	std::vector<SceneObject> mStaticSceneObjects;
	std::vector<SceneObject> mRigidSceneObjects;

	std::vector<SceneMaterial> mSceneMaterials;

	std::vector<SceneSubobject> mSceneSubobjects;
};