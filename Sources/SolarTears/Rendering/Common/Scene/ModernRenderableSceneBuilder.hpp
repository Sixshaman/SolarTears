#pragma once

#include "RenderableSceneBuilderBase.hpp"

class ModernRenderableScene;

class ModernRenderableSceneBuilder: public RenderableSceneBuilderBase
{
public:
	ModernRenderableSceneBuilder(ModernRenderableScene* sceneToBuild);
	~ModernRenderableSceneBuilder();

	void BakeSceneFirstPart();
	void BakeSceneSecondPart();

protected:
	virtual void   CreateVertexBufferPrerequisiteObject(size_t vertexDataSize)                      = 0;
	virtual void   CreateIndexBufferPrerequisiteObject(size_t indexDataSize)                        = 0;
	virtual void   CreateConstantBufferPrerequisiteObject(size_t constantDataSize)                  = 0;
	virtual size_t CreateTexturePrerequisiteObjects(const std::vector<std::wstring>& sceneTextures) = 0;

	virtual void       CreateIntermediateBuffer(uint64_t intermediateBufferSize)       = 0;
	virtual std::byte* MapIntermediateBuffer()                                   const = 0;
	virtual void       UnmapIntermediateBuffer()                                 const = 0;

private:
	void CreateSceneMeshMetadata(std::vector<std::wstring>& sceneTexturesVec);

	size_t CreateBufferPrerequisiteObjects(size_t intermediateBufferSize);
	size_t CreateTexturePrerequisiteObjects(const std::vector<std::wstring>& sceneTextures, size_t intermediateBufferSize);

	void FillIntermediateBufferData();

protected:
	ModernRenderableScene* mSceneToBuild;

	std::vector<uint8_t>               mTextureData;
	std::vector<RenderableSceneVertex> mVertexBufferData;
	std::vector<RenderableSceneIndex>  mIndexBufferData;

	uint64_t mVertexBufferGpuMemoryOffset;
	uint64_t mIndexBufferGpuMemoryOffset;
	uint64_t mConstantBufferGpuMemoryOffset;

	uint64_t mIntermediateBufferVertexDataOffset;
	uint64_t mIntermediateBufferIndexDataOffset;
	uint64_t mIntermediateBufferTextureDataOffset;
};