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

	//Describes the info for a single drawcall
	struct SceneSubmesh
	{
		uint32_t IndexCount;    //Index count to draw
		uint32_t FirstIndex;    //First index in the index buffer to draw
		int32_t  VertexOffset;  //First vertex in the vertex buffer to draw
		uint32_t MaterialIndex; //The per-scene material index to apply to the mesh
	};

	//Describes a single composite object
	struct SceneMesh
	{
		uint32_t PerObjectDataIndex;    //The index of the object data for the drawcall
		uint32_t InstanceCount;         //The instance count of the mesh
		uint32_t FirstSubmeshIndex;     //The start of the submesh span in mSceneSubmeshes
		uint32_t AfterLastSubmeshIndex; //The end of the submesh span in mSceneSubmeshes
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

	virtual void UpdateFrameData(const FrameDataUpdateInfo& frameUpdate, uint64_t frameNumber)                      = 0;
	virtual void UpdateRigidSceneObjects(const std::span<ObjectDataUpdateInfo> objectUpdates, uint64_t frameNumber) = 0;

protected:
	PerObjectData PackObjectData(const SceneObjectLocation& sceneObjectLocation)                          const;
	PerFrameData  PackFrameData(const SceneObjectLocation& cameraLocation, DirectX::FXMMATRIX ProjMatrix) const;

protected:
	std::vector<SceneMesh>    mSceneMeshes;    //All meshes registered in scene
	std::vector<SceneSubmesh> mSceneSubmeshes; //All submeshes registered in scene
	
	Span<uint32_t> mStaticUniqueMeshSpan; //Meshes that have the positional data baked into vertices
	Span<uint32_t> mNonStaticMeshSpan;    //Meshes with positional data stored in a constant buffer
};