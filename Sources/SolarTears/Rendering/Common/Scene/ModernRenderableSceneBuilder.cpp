#include "ModernRenderableSceneBuilder.hpp"
#include "ModernRenderableScene.hpp"
#include "../RenderingUtils.hpp"

ModernRenderableSceneBuilder::ModernRenderableSceneBuilder(ModernRenderableScene* sceneToBuild, size_t texturePlacementAlignment): BaseRenderableSceneBuilder(sceneToBuild), mModernSceneToBuild(sceneToBuild), mTexturePlacementAlignment(texturePlacementAlignment)
{
	mVertexBufferGpuMemoryOffset   = 0;
	mIndexBufferGpuMemoryOffset    = 0;
	mConstantBufferGpuMemoryOffset = 0;
	mUploadBufferMemoryOffset      = 0;

	mIntermediateBufferSize = 0;
}

ModernRenderableSceneBuilder::~ModernRenderableSceneBuilder()
{
}

void ModernRenderableSceneBuilder::Bake()
{
	//Initialize everything needed to create buffers
	InitializeBufferCreationData();

	//Initialize everything needed to create textures
	InitializeTextureCreationData();

	//Create the buffers and textures
	FinalizeBuffersAndTexturesCreation();

	//Create and fill intermediate buffer
	UploadIntermediateData();

	//Fill dynamic buffer data
	InitializeDynamicConstantData();

	//Send the upload commands to GPU and wait
	HandleUploadCommands();
}

void ModernRenderableSceneBuilder::InitializeBufferCreationData()
{
	//Create vertex buffer data
	const size_t vertexDataSize = mVertexBufferData.size() * sizeof(RenderableSceneVertex);
	CreateVertexBufferInfo(vertexDataSize);

	mIntermediateBufferVertexDataOffset = mIntermediateBufferSize;
	mIntermediateBufferSize            += vertexDataSize;


	//Create index buffer data
	const size_t indexDataSize = mIndexBufferData.size() * sizeof(RenderableSceneIndex);
	CreateIndexBufferInfo(indexDataSize);

	mIntermediateBufferIndexDataOffset = mIntermediateBufferSize;
	mIntermediateBufferSize           += indexDataSize;


	//Create static constant buffer data
	mModernSceneToBuild->mMaterialDataSize     = mMaterialData.size() * mModernSceneToBuild->mMaterialChunkDataSize;
	mModernSceneToBuild->mFrameDataSize        = mModernSceneToBuild->mFrameChunkDataSize;
	mModernSceneToBuild->mStaticObjectDataSize = mStaticInstancedObjectCount * mModernSceneToBuild->mObjectChunkDataSize;
	mModernSceneToBuild->mRigidObjectDataSize  = mRigidObjectCount * mModernSceneToBuild->mObjectChunkDataSize;

	const size_t constantDataSize = mModernSceneToBuild->mMaterialDataSize + mModernSceneToBuild->mFrameDataSize + mModernSceneToBuild->mStaticObjectDataSize + mModernSceneToBuild->mRigidObjectDataSize;
	CreateConstantBufferInfo(constantDataSize);

	mIntermediateBufferStaticConstantDataOffset = mIntermediateBufferSize;
	mIntermediateBufferSize                    += constantDataSize;

	mStaticConstantData.resize(mModernSceneToBuild->mMaterialDataSize + mModernSceneToBuild->mStaticObjectDataSize);

	std::byte* staticMaterialDataStart = mStaticConstantData.data();
	for(uint32_t materialIndex = 0; materialIndex < (uint32_t)mMaterialData.size(); materialIndex++)
	{
		std::byte* materialDataPointer = staticMaterialDataStart + materialIndex * mModernSceneToBuild->mMaterialChunkDataSize;
		memcpy(materialDataPointer, &mMaterialData[materialIndex], sizeof(RenderableSceneMaterial));
	}

	const uint32_t initialDataStaticObjectsOffset = 0;
	std::byte* staticObjectDataStart = mStaticConstantData.data() + mModernSceneToBuild->mMaterialDataSize;
	for(uint32_t staticObjectIndex = 0; staticObjectIndex < mStaticInstancedObjectCount; staticObjectIndex++)
	{
		const BaseRenderableScene::PerObjectData perObjectData = mModernSceneToBuild->PackObjectData(mInitialObjectData[initialDataStaticObjectsOffset + staticObjectIndex]);

		std::byte* staticDataPointer = staticObjectDataStart + staticObjectIndex * mModernSceneToBuild->mStaticObjectDataSize;
		memcpy(staticDataPointer, &perObjectData, sizeof(BaseRenderableScene::PerObjectData));
	}


	//Create non-static constant buffer data
	const size_t uploadPerObjectDataSize = mModernSceneToBuild->mRigidObjectDataSize * Utils::InFlightFrameCount;
	const size_t uploadPerFrameDataSize  = mModernSceneToBuild->mFrameDataSize * Utils::InFlightFrameCount;
	CreateUploadBufferInfo(uploadPerObjectDataSize + uploadPerFrameDataSize);
}

void ModernRenderableSceneBuilder::InitializeTextureCreationData()
{
	mIntermediateBufferTextureDataOffset = Utils::AlignMemory(mIntermediateBufferSize, mTexturePlacementAlignment);
	mIntermediateBufferSize              = mIntermediateBufferTextureDataOffset;

	mTextureData.clear();

	AllocateTextureMetadataArrays(mTexturesToLoad.size());
	for(size_t textureIndex = 0; textureIndex < mTexturesToLoad.size(); textureIndex++)
	{
		std::vector<std::byte> textureData;
		LoadTextureFromFile(mTexturesToLoad[textureIndex], mIntermediateBufferSize, textureIndex, textureData);

		mTextureData.insert(mTextureData.end(), textureData.begin(), textureData.end());
		mIntermediateBufferSize += textureData.size();
	}
}

void ModernRenderableSceneBuilder::FinalizeBuffersAndTexturesCreation()
{
	FinishBufferCreation();
	FinishTextureCreation();
}

void ModernRenderableSceneBuilder::UploadIntermediateData()
{
	CreateIntermediateBuffer();

	const uint64_t vertexDataSize         = mVertexBufferData.size()   * sizeof(RenderableSceneVertex);
	const uint64_t indexDataSize          = mIndexBufferData.size()    * sizeof(RenderableSceneIndex);
	const uint64_t staticConstantDataSize = mStaticConstantData.size() * sizeof(std::byte);
	const uint64_t textureDataSize        = mTextureData.size()        * sizeof(std::byte);

	std::byte* bufferDataBytes = MapIntermediateBuffer();

	memcpy(bufferDataBytes + mIntermediateBufferVertexDataOffset,         mVertexBufferData.data(),   vertexDataSize);
	memcpy(bufferDataBytes + mIntermediateBufferIndexDataOffset,          mIndexBufferData.data(),    indexDataSize);
	memcpy(bufferDataBytes + mIntermediateBufferStaticConstantDataOffset, mStaticConstantData.data(), staticConstantDataSize);
	memcpy(bufferDataBytes + mIntermediateBufferTextureDataOffset,        mTextureData.data(),        textureDataSize);

	UnmapIntermediateBuffer();
}

void ModernRenderableSceneBuilder::InitializeUploadBuffer()
{
	mModernSceneToBuild->mSceneUploadDataBufferPointer = MapUploadBuffer();


	//Prepare update data
	mModernSceneToBuild->mPrevFrameRigidMeshUpdates.resize((size_t)mRigidObjectCount * Utils::InFlightFrameCount + 1); //1 for each potential update and terminating (-1)
	mModernSceneToBuild->mNextFrameRigidMeshUpdates.resize((size_t)mRigidObjectCount * Utils::InFlightFrameCount + 1); //1 for each potential update and terminating (-1)

	mModernSceneToBuild->mCurrFrameRigidMeshUpdateIndices.resize(mRigidObjectCount); //1 for each potential update
	mModernSceneToBuild->mCurrFrameUpdatedObjectCount = 0;

	mModernSceneToBuild->mPrevFrameDataToUpdate.resize(mRigidObjectCount); //1 for each potential update
	mModernSceneToBuild->mCurrFrameDataToUpdate.resize(mRigidObjectCount); //1 for each potential update

	mModernSceneToBuild->mPrevFrameRigidMeshUpdates[0] =
	{
		.MeshHandleIndex = (uint32_t)(-1),
		.ObjectDataIndex = (uint32_t)(-1)
	};

	const uint32_t initialDataRigidObjectsOffset = mStaticInstancedObjectCount;
	std::vector<ObjectDataUpdateInfo> rigidObjectUpdates(mRigidObjectCount);
	for(uint32_t rigidUpdateIndex = 0; rigidUpdateIndex < mRigidObjectCount; rigidUpdateIndex++)
	{
		const uint32_t objectIndex = initialDataRigidObjectsOffset + rigidUpdateIndex;
		rigidObjectUpdates[rigidUpdateIndex] = ObjectDataUpdateInfo
		{
			.ObjectId          = objectIndex,
			.NewObjectLocation = mInitialObjectData[objectIndex],
		};
	}

	mModernSceneToBuild->UpdateRigidSceneObjects(rigidObjectUpdates, 0);
}

void ModernRenderableSceneBuilder::HandleUploadCommands()
{
	WriteInitializationCommands();
	SubmitInitializationCommands();
	WaitForInitializationCommands();
}