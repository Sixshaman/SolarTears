#pragma once

#include <DirectXMath.h>
#include "RenderableSceneMisc.hpp"
#include "../../../Core/Scene/Scene.hpp"
#include "../../../Core/Scene/SceneObjectLocation.hpp"
#include <span>

class RenderableSceneBase
{
	friend class RenderableSceneBuilderBase;

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

	enum class SceneDataType: uint32_t
	{
		ObjectData = 0x01,
		FrameData,
		MaterialData,
		TextureData,

		Count
	};

	enum class SceneObjectType: uint8_t
	{
		Static = 0,
		Rigid,

		Count
	};

public:
	RenderableSceneBase(uint32_t maxDirtyFrames);
	~RenderableSceneBase();

	virtual void UpdateRigidSceneObjects(const FrameDataUpdateInfo& frameUpdate, const std::span<ObjectDataUpdateInfo> objectUpdates) = 0;

protected:
	PerObjectData PackObjectData(const SceneObjectLocation& sceneObjectLocation)  const;
	PerFrameData  PackFrameData(DirectX::FXMMATRIX View, DirectX::FXMMATRIX Proj) const;

	size_t          ObjectArrayIndex(const RenderableSceneObjectHandle meshHandle);
	SceneObjectType ObjectType(const RenderableSceneObjectHandle meshHandle);

protected:
	std::vector<SceneObject> mStaticSceneObjects;
	std::vector<SceneObject> mRigidSceneObjects;

	std::vector<SceneMaterial> mSceneMaterials;

	std::vector<SceneSubobject> mSceneSubobjects;
};