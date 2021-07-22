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

	struct SceneSubmesh
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

	struct StaticSceneObject
	{
		uint32_t FirstSubmeshIndex;
		uint32_t AfterLastSubmeshIndex;
	};

	struct RigidSceneObject
	{
		uint32_t PerObjectDataIndex;
		uint32_t FirstSubmeshIndex;
		uint32_t AfterLastSubmeshIndex;
	};

	enum class SceneDataType: uint32_t
	{
		ObjectData = 0x00,
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

	size_t          ObjectArrayIndex(const RenderableSceneObjectHandle meshHandle) const;
	SceneObjectType ObjectType(const RenderableSceneObjectHandle meshHandle) const;

protected:
	std::vector<StaticSceneObject> mStaticSceneObjects;
	std::vector<RigidSceneObject>  mRigidSceneObjects;

	std::vector<SceneMaterial> mSceneMaterials;

	std::vector<SceneSubmesh> mSceneSubmeshes;
};