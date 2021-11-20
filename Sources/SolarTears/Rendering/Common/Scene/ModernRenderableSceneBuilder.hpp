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
	virtual void CreateVertexBufferInfo(size_t vertexDataSize)     = 0; //Prepare the necessary data for vertex buffer creation
	virtual void CreateIndexBufferInfo(size_t indexDataSize)       = 0; //Prepare the necessary data for index buffer creation
	virtual void CreateConstantBufferInfo(size_t constantDataSize) = 0; //Prepare the necessary data for constant buffer creation
	virtual void CreateUploadBufferInfo(size_t constantDataSize)   = 0; //Prepare the necessary data for upload buffer creation (intermediate buffer for dynamic constant data)

	virtual void AllocateTextureMetadataArrays(size_t textureCount)                                                                                                              = 0;
	virtual void LoadTextureFromFile(const std::wstring& textureFilename, uint64_t currentIntermediateBufferOffset, size_t textureIndex, std::vector<std::byte>& outTextureData) = 0;

	virtual void FinishBufferCreation()  = 0;
	virtual void FinishTextureCreation() = 0;

	virtual std::byte* MapUploadBuffer() = 0;

	virtual void       CreateIntermediateBuffer()      = 0;
	virtual std::byte* MapIntermediateBuffer()   const = 0;
	virtual void       UnmapIntermediateBuffer() const = 0;

	virtual void WriteInitializationCommands()   const = 0;
	virtual void SubmitInitializationCommands()  const = 0;
	virtual void WaitForInitializationCommands() const = 0;

private:
	//Initialize the buffer data and API-specific info to create buffers
	void InitializeBufferCreationData();
	
	//Initialize the texture data and API-specific info to create textures
	void InitializeTextureCreationData();
	
	//Create and allocate buffers and textures
	void FinalizeBuffersAndTexturesCreation();
	
	//Upload the static data to the intermediate buffer
	void UploadIntermediateData();
	
	//Initialize the CPU-visible buffer for dynamic data
	void InitializeUploadBuffer();

	//Send the upload commands to GPU and wait
	void HandleUploadCommands();

protected:
	ModernRenderableScene* mModernSceneToBuild;

	std::vector<std::byte> mTextureData;
	std::vector<std::byte> mStaticConstantData;

	size_t mTexturePlacementAlignment;

	uint64_t mVertexBufferGpuMemoryOffset;
	uint64_t mIndexBufferGpuMemoryOffset;
	uint64_t mConstantBufferGpuMemoryOffset;
	uint64_t mUploadBufferMemoryOffset;

	uint64_t mIntermediateBufferVertexDataOffset;
	uint64_t mIntermediateBufferIndexDataOffset;
	uint64_t mIntermediateBufferStaticConstantDataOffset;
	uint64_t mIntermediateBufferTextureDataOffset;

	size_t mIntermediateBufferSize;
};