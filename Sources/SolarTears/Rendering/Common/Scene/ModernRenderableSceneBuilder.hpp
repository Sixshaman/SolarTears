#pragma once

#include "RenderableSceneDescription.hpp"

class ModernRenderableScene;

class ModernRenderableSceneBuilder
{
public:
	ModernRenderableSceneBuilder(ModernRenderableScene* sceneToBuild);
	~ModernRenderableSceneBuilder();

	void BakeSceneFirstPart(const RenderableSceneDescription& sceneDescription);
	void BakeSceneSecondPart();

protected:
	virtual void AllocateTextureMetadataArrays(size_t textureCount)                                                                                                              = 0;
	virtual void LoadTextureFromFile(const std::wstring& textureFilename, uint64_t currentIntermediateBufferOffset, size_t textureIndex, std::vector<std::byte>& outTextureData) = 0;

	virtual void PreCreateVertexBuffer(size_t vertexDataSize)     = 0;
	virtual void PreCreateIndexBuffer(size_t indexDataSize)       = 0;
	virtual void PreCreateConstantBuffer(size_t constantDataSize) = 0;

	virtual void FinishBufferCreation()  = 0;
	virtual void FinishTextureCreation() = 0;

	virtual std::byte* MapConstantBuffer() = 0;

	virtual void       CreateIntermediateBuffer(uint64_t intermediateBufferSize)       = 0;
	virtual std::byte* MapIntermediateBuffer()                                   const = 0;
	virtual void       UnmapIntermediateBuffer()                                 const = 0;

	virtual void WriteInitializationCommands()   const = 0;
	virtual void SubmitInitializationCommands()  const = 0;
	virtual void WaitForInitializationCommands() const = 0;

private:
	void CreateSceneMeshMetadata(std::vector<std::wstring>& sceneTexturesVec);

	size_t PreCreateBuffers(size_t intermediateBufferSize);
	size_t PreCreateTextures(const std::vector<std::wstring>& sceneTextures, size_t intermediateBufferSize);

	void FillIntermediateBufferData();

protected:
	ModernRenderableScene* mSceneToBuild;

	std::vector<std::byte>             mTextureData;
	std::vector<RenderableSceneVertex> mVertexBufferData;
	std::vector<RenderableSceneIndex>  mIndexBufferData;

	uint64_t mVertexBufferGpuMemoryOffset;
	uint64_t mIndexBufferGpuMemoryOffset;
	uint64_t mConstantBufferGpuMemoryOffset;

	uint64_t mIntermediateBufferVertexDataOffset;
	uint64_t mIntermediateBufferIndexDataOffset;
	uint64_t mIntermediateBufferTextureDataOffset;
};