#pragma once

#include "RenderableSceneBase.hpp"

//Class for scene functions common to OpenGL and D3D11
class ClassicRenderableScene: public RenderableSceneBase
{
	friend class ClassicRenderableSceneBuilder;

protected:
	struct SceneSubobject
	{
		uint32_t IndexCount;
		uint32_t FirstIndex;
		int32_t  VertexOffset;
		uint32_t TextureDescriptorSetIndex;
	};

	struct MeshSubobjectRange
	{
		uint32_t FirstSubobjectIndex;
		uint32_t AfterLastSubobjectIndex;
	};

public:
	ClassicRenderableScene();
	~ClassicRenderableScene();

protected:
	void FinalizeSceneUpdatingImpl();

protected:
	//Created from outside
	std::vector<MeshSubobjectRange> mSceneMeshes;
	std::vector<SceneSubobject>     mSceneSubobjects;
};