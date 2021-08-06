#include "ModernRenderableSceneBuilder.hpp"
#include "ModernRenderableScene.hpp"
#include "../RenderingUtils.hpp"

ModernRenderableSceneBuilder::ModernRenderableSceneBuilder(ModernRenderableScene* sceneToBuild, size_t texturePlacementAlignment): BaseRenderableSceneBuilder(sceneToBuild), mModernSceneToBuild(sceneToBuild), mTexturePlacementAlignment(texturePlacementAlignment)
{
	mVertexBufferGpuMemoryOffset          = 0;
	mIndexBufferGpuMemoryOffset           = 0;
	mStaticConstantBufferGpuMemoryOffset  = 0;
	mDynamicConstantBufferGpuMemoryOffset = 0;
}

ModernRenderableSceneBuilder::~ModernRenderableSceneBuilder()
{
}

void ModernRenderableSceneBuilder::Bake()
{
	size_t intermediateBufferSize = 0;

	//Create buffers for model and uniform data
	PreCreateGeometryBuffers(&intermediateBufferSize);

	//Create buffers for constant data
	PreCreateConstantDataBuffers(&intermediateBufferSize);

	//Load texture images
	PreCreateTextures(&intermediateBufferSize);

	FinishBufferCreation();
	mModernSceneToBuild->mSceneConstantDataBufferPointer = MapDynamicConstantBuffer();

	FinishTextureCreation();

	//Create intermediate buffers
	CreateIntermediateBuffer(intermediateBufferSize);

	//Fill intermediate buffers
	FillIntermediateBufferData();

	//Initialize everything needed to update dynamic constant buffers
	InitializeDynamicConstantData();


	WriteInitializationCommands();
	SubmitInitializationCommands();
	WaitForInitializationCommands();
}

void ModernRenderableSceneBuilder::PreCreateGeometryBuffers(size_t* inoutIntermediateBufferSize)
{
	assert(inoutIntermediateBufferSize != nullptr);
	size_t intermediateBufferSize = *inoutIntermediateBufferSize;


	const size_t vertexDataSize = mVertexBufferData.size() * sizeof(RenderableSceneVertex);
	PreCreateVertexBuffer(vertexDataSize);

	mIntermediateBufferVertexDataOffset = intermediateBufferSize;
	intermediateBufferSize             += vertexDataSize;


	const size_t indexDataSize = mIndexBufferData.size() * sizeof(RenderableSceneIndex);
	PreCreateIndexBuffer(indexDataSize);

	mIntermediateBufferIndexDataOffset = intermediateBufferSize;
	intermediateBufferSize            += indexDataSize;


	*inoutIntermediateBufferSize = intermediateBufferSize;
}

void ModernRenderableSceneBuilder::PreCreateConstantDataBuffers(size_t* inoutIntermediateBufferSize)
{
	assert(inoutIntermediateBufferSize != nullptr);
	size_t intermediateBufferSize = *inoutIntermediateBufferSize;


	const size_t staticConstantDataSize = mMaterialData.size() * mModernSceneToBuild->mMaterialChunkDataSize + mInitialStaticInstancedObjectData.size() * mModernSceneToBuild->mObjectChunkDataSize;
	PreCreateStaticConstantBuffer(staticConstantDataSize);

	mIntermediateBufferStaticConstantDataOffset = intermediateBufferSize;
	intermediateBufferSize                     += staticConstantDataSize;

	mModernSceneToBuild->mMaterialDataSize     = mMaterialData.size() * mModernSceneToBuild->mMaterialChunkDataSize;
	mModernSceneToBuild->mStaticObjectDataSize = mStaticConstantData.size() * mModernSceneToBuild->mObjectChunkDataSize;


	const size_t constantPerObjectDataSize = mInitialRigidObjectData.size() * mModernSceneToBuild->mObjectChunkDataSize * Utils::InFlightFrameCount;
	const size_t constantPerFrameDataSize  = mModernSceneToBuild->mFrameChunkDataSize * Utils::InFlightFrameCount;
	PreCreateDynamicConstantBuffer(constantPerObjectDataSize + constantPerFrameDataSize);


	mStaticConstantData.resize(staticConstantDataSize);

	std::byte* staticConstantDataPointer = mStaticConstantData.data() + mModernSceneToBuild->GetBaseMaterialDataOffset();
	for(uint32_t materialIndex = 0; materialIndex < (uint32_t)mMaterialData.size(); materialIndex++)
	{
		memcpy(staticConstantDataPointer, &mMaterialData[materialIndex], sizeof(RenderableSceneMaterial));
		staticConstantDataPointer += mModernSceneToBuild->mMaterialChunkDataSize;
	}

	staticConstantDataPointer = mStaticConstantData.data() + mModernSceneToBuild->GetBaseStaticObjectDataOffset();
	for(uint32_t staticObjectIndex = 0; staticObjectIndex < (uint32_t)mInitialStaticInstancedObjectData.size(); staticObjectIndex++)
	{
		const BaseRenderableScene::PerObjectData perObjectData = mModernSceneToBuild->PackObjectData(mInitialStaticInstancedObjectData[staticObjectIndex]);

		memcpy(staticConstantDataPointer, &perObjectData, sizeof(BaseRenderableScene::PerObjectData));
		staticConstantDataPointer += mModernSceneToBuild->mObjectChunkDataSize;
	}

	*inoutIntermediateBufferSize = intermediateBufferSize;
}

void ModernRenderableSceneBuilder::PreCreateTextures(size_t* inoutIntermediateBufferSize)
{
	assert(inoutIntermediateBufferSize != nullptr);
	size_t intermediateBufferSize = *inoutIntermediateBufferSize;

	mIntermediateBufferTextureDataOffset = Utils::AlignMemory(intermediateBufferSize, mTexturePlacementAlignment);
	intermediateBufferSize = mIntermediateBufferTextureDataOffset;

	mTextureData.clear();

	AllocateTextureMetadataArrays(mTexturesToLoad.size());
	for(size_t textureIndex = 0; textureIndex < mTexturesToLoad.size(); textureIndex++)
	{
		std::vector<std::byte> textureData;
		LoadTextureFromFile(mTexturesToLoad[textureIndex], intermediateBufferSize, textureIndex, textureData);

		mTextureData.insert(mTextureData.end(), textureData.begin(), textureData.end());
		intermediateBufferSize += textureData.size();
	}

	*inoutIntermediateBufferSize = intermediateBufferSize;
}

void ModernRenderableSceneBuilder::FillIntermediateBufferData()
{
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