#include "ModernRenderableSceneBuilder.hpp"
#include "ModernRenderableScene.hpp"
#include "../RenderingUtils.hpp"

ModernRenderableSceneBuilder::ModernRenderableSceneBuilder(ModernRenderableScene* sceneToBuild, size_t texturePlacementAlignment): BaseRenderableSceneBuilder(sceneToBuild), mModernSceneToBuild(sceneToBuild), mTexturePlacementAlignment(texturePlacementAlignment)
{
	mVertexBufferGpuMemoryOffset          = 0;
	mIndexBufferGpuMemoryOffset           = 0;
	mStaticConstantBufferGpuMemoryOffset  = 0;
	mDynamicConstantBufferGpuMemoryOffset = 0;

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
	const size_t staticConstantDataSize = mMaterialData.size() * mModernSceneToBuild->mMaterialChunkDataSize + mInitialStaticInstancedObjectData.size() * mModernSceneToBuild->mObjectChunkDataSize;
	CreateStaticConstantBufferInfo(staticConstantDataSize);

	mIntermediateBufferStaticConstantDataOffset = mIntermediateBufferSize;
	mIntermediateBufferSize                    += staticConstantDataSize;

	mModernSceneToBuild->mMaterialDataSize     = mMaterialData.size() * mModernSceneToBuild->mMaterialChunkDataSize;
	mModernSceneToBuild->mStaticObjectDataSize = mStaticConstantData.size() * mModernSceneToBuild->mObjectChunkDataSize;

	mStaticConstantData.resize(staticConstantDataSize);

	std::byte* materialDataStart = mStaticConstantData.data() + mModernSceneToBuild->GetBaseMaterialDataOffset();
	for(uint32_t materialIndex = 0; materialIndex < (uint32_t)mMaterialData.size(); materialIndex++)
	{
		std::byte* materialDataPointer = materialDataStart + materialIndex * mModernSceneToBuild->mMaterialChunkDataSize;
		memcpy(materialDataPointer, &mMaterialData[materialIndex], sizeof(RenderableSceneMaterial));
	}

	std::byte* staticObjectDataStart = mStaticConstantData.data() + mModernSceneToBuild->GetBaseStaticObjectDataOffset();
	for(uint32_t staticObjectIndex = 0; staticObjectIndex < (uint32_t)mInitialStaticInstancedObjectData.size(); staticObjectIndex++)
	{
		const BaseRenderableScene::PerObjectData perObjectData = mModernSceneToBuild->PackObjectData(mInitialStaticInstancedObjectData[staticObjectIndex]);

		std::byte* staticDataPointer = staticObjectDataStart + staticObjectIndex * mModernSceneToBuild->mStaticObjectDataSize;
		memcpy(staticDataPointer, &perObjectData, sizeof(BaseRenderableScene::PerObjectData));
	}


	//Create non-static constant buffer data
	const size_t perObjectDataSize = mInitialRigidObjectData.size() * mModernSceneToBuild->mObjectChunkDataSize * Utils::InFlightFrameCount;
	const size_t perFrameDataSize  = mModernSceneToBuild->mFrameChunkDataSize * Utils::InFlightFrameCount;
	CreateDynamicConstantBufferInfo(perObjectDataSize + perFrameDataSize);

	mDynamicConstantBufferGpuMemoryOffset = 0;
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

void ModernRenderableSceneBuilder::InitializeDynamicConstantData()
{
	mModernSceneToBuild->mSceneConstantDataBufferPointer = MapDynamicConstantBuffer();


	//Prepare update data
	mModernSceneToBuild->mPrevFrameRigidMeshUpdates.resize(mInitialRigidObjectData.size() * Utils::InFlightFrameCount + 1); //1 for each potential update and terminating (-1)
	mModernSceneToBuild->mNextFrameRigidMeshUpdates.resize(mInitialRigidObjectData.size() * Utils::InFlightFrameCount + 1); //1 for each potential update and terminating (-1)

	mModernSceneToBuild->mCurrFrameRigidMeshUpdateIndices.resize(mInitialRigidObjectData.size()); //1 for each potential update

	mModernSceneToBuild->mPrevFrameDataToUpdate.resize(mInitialRigidObjectData.size()); //1 for each potential update
	mModernSceneToBuild->mCurrFrameDataToUpdate.resize(mInitialRigidObjectData.size()); //1 for each potential update

	mModernSceneToBuild->mPrevFrameRigidMeshUpdates[0] =
	{
		.MeshHandleIndex = (uint32_t)(-1),
		.ObjectDataIndex = (uint32_t)(-1)
	};

	std::vector<ObjectDataUpdateInfo> rigidObjectUpdates(mInitialRigidObjectData.size());
	for(size_t rigidUpdateIndex = 0; rigidUpdateIndex < mInitialRigidObjectData.size(); rigidUpdateIndex++)
	{
		rigidObjectUpdates[rigidUpdateIndex] = ObjectDataUpdateInfo
		{
			.ObjectId          = RenderableSceneObjectHandle(rigidUpdateIndex, SceneObjectType::Rigid),
			.NewObjectLocation = mInitialRigidObjectData[rigidUpdateIndex],
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