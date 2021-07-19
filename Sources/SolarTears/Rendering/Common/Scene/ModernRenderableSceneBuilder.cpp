#include "ModernRenderableSceneBuilder.hpp"
#include "ModernRenderableScene.hpp"
#include "../RenderingUtils.hpp"

ModernRenderableSceneBuilder::ModernRenderableSceneBuilder(ModernRenderableScene* sceneToBuild): RenderableSceneBuilderBase(sceneToBuild), mSceneToBuild(sceneToBuild)
{
	mVertexBufferGpuMemoryOffset   = 0;
	mIndexBufferGpuMemoryOffset    = 0;
	mConstantBufferGpuMemoryOffset = 0;
}

ModernRenderableSceneBuilder::~ModernRenderableSceneBuilder()
{
}

void ModernRenderableSceneBuilder::BakeSceneFirstPart()
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

void ModernRenderableSceneBuilder::CreateSceneMeshMetadata(std::vector<std::wstring>& sceneTexturesVec)
{
	mVertexBufferData.clear();
	mIndexBufferData.clear();

	mSceneToBuild->mRigidSceneObjects.clear();
	mSceneToBuild->mStaticSceneObjects.clear();
	mSceneToBuild->mSceneSubobjects.clear();

	std::unordered_map<std::wstring, size_t> textureVecIndices;
	for(auto it = mSceneTextures.begin(); it != mSceneTextures.end(); ++it)
	{
		sceneTexturesVec.push_back(*it);
		textureVecIndices[*it] = sceneTexturesVec.size() - 1;
	}

	//Create scene subobjects
	for(size_t i = 0; i < mSceneMeshes.size(); i++)
	{
		//Need to change this in case of multiple subobjects for mesh
		ModernRenderableScene::MeshSubobjectRange subobjectRange;
		subobjectRange.FirstSubobjectIndex     = (uint32_t)i;
		subobjectRange.AfterLastSubobjectIndex = (uint32_t)(i + 1);

		mSceneToBuild->mSceneMeshes.push_back(subobjectRange);

		ModernRenderableScene::SceneSubobject subobject;
		subobject.FirstIndex                = (uint32_t)mIndexBufferData.size();
		subobject.IndexCount                = (uint32_t)mSceneMeshes[i].Indices.size();
		subobject.VertexOffset              = (int32_t)mVertexBufferData.size();
		subobject.TextureDescriptorSetIndex = (uint32_t)textureVecIndices[mSceneMeshes[i].TextureFilename];

		mVertexBufferData.insert(mVertexBufferData.end(), mSceneMeshes[i].Vertices.begin(), mSceneMeshes[i].Vertices.end());
		mIndexBufferData.insert(mIndexBufferData.end(), mSceneMeshes[i].Indices.begin(), mSceneMeshes[i].Indices.end());

		mSceneMeshes[i].Vertices.clear();
		mSceneMeshes[i].Indices.clear();

		mSceneToBuild->mSceneSubobjects.push_back(subobject);
	}

	mSceneToBuild->mScenePerObjectData.resize(mSceneToBuild->mSceneMeshes.size());
	mSceneToBuild->mScheduledSceneUpdates.resize(mSceneToBuild->mSceneMeshes.size() + 1); //1 for each subobject and 1 for frame data
	mSceneToBuild->mObjectDataScheduledUpdateIndices.resize(mSceneToBuild->mSceneSubobjects.size(), (uint32_t)(-1));
	mSceneToBuild->mFrameDataScheduledUpdateIndex = (uint32_t)(-1);
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
