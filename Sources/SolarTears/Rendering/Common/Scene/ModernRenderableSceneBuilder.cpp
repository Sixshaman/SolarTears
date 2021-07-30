#include "ModernRenderableSceneBuilder.hpp"
#include "ModernRenderableScene.hpp"
#include "../RenderingUtils.hpp"

ModernRenderableSceneBuilder::ModernRenderableSceneBuilder(ModernRenderableScene* sceneToBuild): BaseRenderableSceneBuilder(sceneToBuild), mModernSceneToBuild(sceneToBuild)
{
	mVertexBufferGpuMemoryOffset   = 0;
	mIndexBufferGpuMemoryOffset    = 0;
	mConstantBufferGpuMemoryOffset = 0;
}

ModernRenderableSceneBuilder::~ModernRenderableSceneBuilder()
{
}

void ModernRenderableSceneBuilder::Bake()
{
}

void ModernRenderableSceneBuilder::BakeSceneFirstPart(const RenderableSceneDescription& sceneDescription)
{
	//Create scene subobjects
	std::vector<std::wstring> sceneTexturesVec;
	CreateSceneMeshMetadata(sceneTexturesVec);

	size_t intermediateBufferSize = 0;

	//Create buffers for model and uniform data
	intermediateBufferSize = PreCreateBuffers(intermediateBufferSize);

	//Load texture images
	intermediateBufferSize = PreCreateTextures(sceneTexturesVec, intermediateBufferSize);

	FinishBufferCreation();
	mSceneToBuild->mSceneConstantDataBufferPointer = MapConstantBuffer();

	FinishTextureCreation();

	//Create intermediate buffers
	CreateIntermediateBuffer(intermediateBufferSize);

	//Fill intermediate buffers
	FillIntermediateBufferData();
}

void ModernRenderableSceneBuilder::BakeSceneSecondPart()
{
	WriteInitializationCommands();
	SubmitInitializationCommands();
	WaitForInitializationCommands();
}

void ModernRenderableSceneBuilder::CreateSceneMeshMetadata()
{
	



	mModernSceneToBuild->mScenePerObjectData.resize(mSceneToBuild->mSceneMeshes.size());
	mModernSceneToBuild->mScheduledSceneUpdates.resize(mSceneToBuild->mSceneMeshes.size() + 1); //1 for each subobject and 1 for frame data


}

size_t ModernRenderableSceneBuilder::PreCreateBuffers(size_t currentIntermediateBufferSize)
{
	size_t intermediateBufferSize = currentIntermediateBufferSize;


	const size_t vertexDataSize = mVertexBufferData.size() * sizeof(RenderableSceneVertex);
	PreCreateVertexBuffer(vertexDataSize);

	mIntermediateBufferVertexDataOffset = intermediateBufferSize;
	intermediateBufferSize             += vertexDataSize;


	const size_t indexDataSize = mIndexBufferData.size() * sizeof(RenderableSceneIndex);
	PreCreateIndexBuffer(indexDataSize);

	mIntermediateBufferIndexDataOffset = intermediateBufferSize;
	intermediateBufferSize            += indexDataSize;


	const size_t constantPerObjectDataSize = mSceneToBuild->mScenePerObjectData.size() * mSceneToBuild->mGBufferObjectChunkDataSize * Utils::InFlightFrameCount;
	const size_t constantPerFrameDataSize  = mSceneToBuild->mGBufferFrameChunkDataSize * Utils::InFlightFrameCount;
	PreCreateConstantBuffer(constantPerObjectDataSize + constantPerFrameDataSize);

	mSceneToBuild->mSceneDataConstantObjectBufferOffset = 0;
	mSceneToBuild->mSceneDataConstantFrameBufferOffset  = mSceneToBuild->mSceneDataConstantObjectBufferOffset + constantPerObjectDataSize;


	return intermediateBufferSize;
}

size_t ModernRenderableSceneBuilder::PreCreateTextures(const std::vector<std::wstring>& sceneTextures, size_t intermediateBufferSize)
{
	mIntermediateBufferTextureDataOffset = Utils::AlignMemory(intermediateBufferSize, mTexturePlacementAlignment);
	size_t currentIntermediateBufferSize = mIntermediateBufferTextureDataOffset;

	mTextureData.clear();

	AllocateTextureMetadataArrays(sceneTextures.size());
	for(size_t i = 0; i < sceneTextures.size(); i++)
	{
		std::vector<std::byte> textureData;
		LoadTextureFromFile(sceneTextures[i], currentIntermediateBufferSize, i, textureData);

		mTextureData.insert(mTextureData.end(), textureData.begin(), textureData.end());
		currentIntermediateBufferSize += textureData.size();
	}

	return currentIntermediateBufferSize;
}

void ModernRenderableSceneBuilder::FillIntermediateBufferData()
{
	const uint64_t textureDataSize = mTextureData.size()      * sizeof(uint8_t);
	const uint64_t vertexDataSize  = mVertexBufferData.size() * sizeof(RenderableSceneVertex);
	const uint64_t indexDataSize   = mIndexBufferData.size()  * sizeof(RenderableSceneIndex);

	std::byte* bufferDataBytes = MapIntermediateBuffer();
	memcpy(bufferDataBytes + mIntermediateBufferVertexDataOffset,  mVertexBufferData.data(), vertexDataSize);
	memcpy(bufferDataBytes + mIntermediateBufferIndexDataOffset,   mIndexBufferData.data(),  indexDataSize);
	memcpy(bufferDataBytes + mIntermediateBufferTextureDataOffset, mTextureData.data(),      textureDataSize);

	UnmapIntermediateBuffer();
}
