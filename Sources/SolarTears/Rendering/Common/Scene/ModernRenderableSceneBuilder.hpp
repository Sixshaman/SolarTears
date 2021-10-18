#pragma once

#include "RenderableSceneDescription.hpp"
#include "BaseRenderableSceneBuilder.hpp"

class ModernRenderableScene;

class ModernRenderableSceneBuilder: public BaseRenderableSceneBuilder
{
public:
	ModernRenderableSceneBuilder(ModernRenderableScene* sceneToBuild, size_t texturePlacementAlignment);
	~ModernRenderableSceneBuilder();

protected:
	void Bake() override;

protected:
	virtual void PreCreateVertexBuffer(size_t vertexDataSize)            = 0; //Prepare the necessary data for vertex buffer creation
	virtual void PreCreateIndexBuffer(size_t indexDataSize)              = 0; //Prepare the necessary data for index buffer creation
	virtual void PreCreateStaticConstantBuffer(size_t constantDataSize)  = 0; //Prepare the necessary data for static constant buffer creation (static meshes, materials)
	virtual void PreCreateDynamicConstantBuffer(size_t constantDataSize) = 0; //Prepare the necessary data for dynamic constant buffer creation creation (rigid moving meshes)

	virtual void AllocateTextureMetadataArrays(size_t textureCount)                                                                                                              = 0;
	virtual void LoadTextureFromFile(const std::wstring& textureFilename, uint64_t currentIntermediateBufferOffset, size_t textureIndex, std::vector<std::byte>& outTextureData) = 0;

	virtual void FinishBufferCreation()  = 0;
	virtual void FinishTextureCreation() = 0;

	virtual std::byte* MapDynamicConstantBuffer() = 0;

	virtual void       CreateIntermediateBuffer(uint64_t intermediateBufferSize)       = 0;
	virtual std::byte* MapIntermediateBuffer()                                   const = 0;
	virtual void       UnmapIntermediateBuffer()                                 const = 0;

	virtual void WriteInitializationCommands()   const = 0;
	virtual void SubmitInitializationCommands()  const = 0;
	virtual void WaitForInitializationCommands() const = 0;

private:
	void PreCreateGeometryBuffers(size_t* inoutIntermediateBufferSize);
	void PreCreateConstantDataBuffers(size_t* inoutIntermediateBufferSize);
	void PreCreateTextures(size_t* inoutIntermediateBufferSize);

	void FillIntermediateBufferData();
	void InitializeDynamicConstantData();

protected:
	ModernRenderableScene* mModernSceneToBuild;

	std::vector<std::byte> mTextureData;
	std::vector<std::byte> mStaticConstantData;

	size_t mTexturePlacementAlignment;

	uint64_t mVertexBufferGpuMemoryOffset;
	uint64_t mIndexBufferGpuMemoryOffset;
	uint64_t mStaticConstantBufferGpuMemoryOffset;
	uint64_t mDynamicConstantBufferGpuMemoryOffset;

	uint64_t mIntermediateBufferVertexDataOffset;
	uint64_t mIntermediateBufferIndexDataOffset;
	uint64_t mIntermediateBufferStaticConstantDataOffset;
	uint64_t mIntermediateBufferTextureDataOffset;
};