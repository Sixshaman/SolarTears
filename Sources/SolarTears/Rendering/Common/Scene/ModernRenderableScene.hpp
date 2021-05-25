#pragma once

#include "RenderableSceneBase.hpp"

class FrameCounter;

//Class for scene functions common to Vulkan and D3D12
class ModernRenderableScene: public RenderableSceneBase
{
protected:
	struct SceneSubobject
	{
		uint32_t IndexCount;
		uint32_t FirstIndex;
		int32_t  VertexOffset;
		uint32_t TextureDescriptorSetIndex;

		//Material data will be set VIA ROOT CONSTANTS
	};

	struct MeshSubobjectRange
	{
		uint32_t FirstSubobjectIndex;
		uint32_t AfterLastSubobjectIndex;
	};

public:
	ModernRenderableScene(const FrameCounter* frameCounter);
	~ModernRenderableScene();

	void FinalizeSceneUpdating() override final;

protected:
	void Init();

	uint64_t CalculatePerObjectDataOffset(uint32_t objectIndex, uint32_t currentFrameResourceIndex) const;
	uint64_t CalculatePerFrameDataOffset(uint32_t currentFrameResourceIndex)                        const;

protected:
	//Created from inside
	const FrameCounter* mFrameCounterRef;

protected:
	//Created from outside
	std::vector<MeshSubobjectRange> mSceneMeshes;
	std::vector<SceneSubobject>     mSceneSubobjects;

	void* mSceneConstantDataBufferPointer; //Constant buffer is persistently mapped

	uint32_t mGBufferObjectChunkDataSize;
	uint32_t mGBufferFrameChunkDataSize;

	uint64_t mSceneDataConstantObjectBufferOffset;
	uint64_t mSceneDataConstantFrameBufferOffset;

	uint64_t mCBufferAlignmentSize;
};