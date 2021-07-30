#pragma once

#include <DirectXMath.h>
#include "RenderableSceneMisc.hpp"
#include "../../../Core/Scene/Scene.hpp"
#include "../../../Core/Scene/SceneObjectLocation.hpp"
#include "../../../Core/DataStructures/Span.hpp"
#include <span>

class BaseRenderableScene
{
	friend class BaseRenderableSceneBuilder;

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

	struct SceneMesh
	{
		uint32_t PerObjectDataIndex;
		uint32_t InstanceCount;
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

public:
	BaseRenderableScene();
	~BaseRenderableScene();

	virtual void UpdateRigidSceneObjects(const FrameDataUpdateInfo& frameUpdate, const std::span<ObjectDataUpdateInfo> objectUpdates) = 0;

protected:
	PerObjectData PackObjectData(const SceneObjectLocation& sceneObjectLocation)  const;
	PerFrameData  PackFrameData(DirectX::FXMMATRIX View, DirectX::FXMMATRIX Proj) const;

protected:
	std::vector<SceneMesh>    mSceneMeshes;
	std::vector<SceneSubmesh> mSceneSubmeshes;

	Span<uint32_t> mStaticMeshSpan;          //Meshes that never move and have the positional data baked into vertices
	Span<uint32_t> mStaticInstancedMeshSpan; //Meshes that never move and have the positional data potentially stored in fast immutable memory
	Span<uint32_t> mRigidMeshSpan;           //Meshes that move and have the positional data stored in CPU-visible memory
};