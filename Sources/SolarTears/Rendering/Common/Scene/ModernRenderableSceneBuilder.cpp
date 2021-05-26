#include "ModernRenderableSceneBuilder.hpp"
#include "ModernRenderableScene.hpp"
#include "../RenderingUtils.hpp"

ModernRenderableSceneBuilder::ModernRenderableSceneBuilder(ModernRenderableScene* sceneToBuild): mSceneToBuild(sceneToBuild)
{
	mVertexBufferGpuMemoryOffset   = 0;
	mIndexBufferGpuMemoryOffset    = 0;
	mConstantBufferGpuMemoryOffset = 0;

	mTexturePlacementAlignment = 1;
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

	//Allocate memory for images and buffers
	AllocateTexturesHeap(memoryAllocator);
	AllocateBuffersHeap(deviceInterface, memoryAllocator);

	CreateBuffers(deviceInterface);
	CreateTextures(deviceInterface);

	CreateBufferViews();

	//Create intermediate buffers
	CreateIntermediateBuffer(intermediateBufferSize);

	//Fill intermediate buffers
	FillIntermediateBufferData();

	//======================================================

	//Vulkan:

	//Create buffers for model and uniform data
	intermediateBufferSize = CreateSceneDataBuffers(deviceQueues, intermediateBufferSize);

	//Load texture images
	intermediateBufferSize = LoadTextureImages(sceneTexturesVec, deviceParameters, intermediateBufferSize);

	//Allocate memory for images and buffers
	AllocateImageMemory(memoryAllocator);
	AllocateBuffersMemory(memoryAllocator);

	CreateImageViews();
}

void ModernRenderableSceneBuilder::BakeSceneSecondPart()
{
	ID3D12CommandAllocator*    commandAllocator = commandLists->GetMainThreadDirectCommandAllocator(0);
	ID3D12GraphicsCommandList* commandList      = commandLists->GetMainThreadDirectCommandList();

	//Baking
	THROW_IF_FAILED(commandAllocator->Reset());
	THROW_IF_FAILED(commandList->Reset(commandAllocator, nullptr));
	
	for(size_t i = 0; i < mSceneToBuild->mSceneTextures.size(); i++)
	{
		for(size_t j = 0; j < mSceneTextureSubresourceFootprints[i].size(); j++)
		{
			D3D12_TEXTURE_COPY_LOCATION dstLocation;
			dstLocation.pResource        = mSceneToBuild->mSceneTextures[i].get();
			dstLocation.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dstLocation.SubresourceIndex = (UINT)j;

			D3D12_TEXTURE_COPY_LOCATION srcLocation;
			srcLocation.pResource       = mIntermediateBuffer.get();
			srcLocation.Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			srcLocation.PlacedFootprint = mSceneTextureSubresourceFootprints[i][j];

			commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);
		}
	}


	std::array sceneBuffers           = {mSceneToBuild->mSceneVertexBuffer.get(),                  mSceneToBuild->mSceneIndexBuffer.get()};
	std::array sceneBufferDataOffsets = {mIntermediateBufferVertexDataOffset,                      mIntermediateBufferIndexDataOffset};
	std::array sceneBufferDataSizes   = {mVertexBufferData.size() * sizeof(RenderableSceneVertex), mIndexBufferData.size() * sizeof(RenderableSceneIndex)};
	std::array sceneBufferStates      = {D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,          D3D12_RESOURCE_STATE_INDEX_BUFFER};
	for(size_t i = 0; i < sceneBuffers.size(); i++)
	{
		commandList->CopyBufferRegion(sceneBuffers[i], 0, mIntermediateBuffer.get(), sceneBufferDataOffsets[i], sceneBufferDataSizes[i]);
	}


	std::vector<D3D12_RESOURCE_BARRIER> barriers;
	barriers.reserve(mSceneToBuild->mSceneTextures.size() + sceneBuffers.size());
	for(int i = 0; i < mSceneToBuild->mSceneTextures.size(); i++)
	{
		D3D12_RESOURCE_BARRIER textureBarrier;
		textureBarrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		textureBarrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		textureBarrier.Transition.pResource   = mSceneToBuild->mSceneTextures[i].get();
		textureBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		textureBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		textureBarrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		barriers.push_back(textureBarrier);
	}

	for(int i = 0; i < sceneBuffers.size(); i++)
	{
		D3D12_RESOURCE_BARRIER textureBarrier;
		textureBarrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		textureBarrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		textureBarrier.Transition.pResource   = sceneBuffers[i];
		textureBarrier.Transition.Subresource = 0;
		textureBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		textureBarrier.Transition.StateAfter  = sceneBufferStates[i];

		barriers.push_back(textureBarrier);
	}

	commandList->ResourceBarrier((UINT)barriers.size(), barriers.data());


	THROW_IF_FAILED(commandList->Close());

	ID3D12CommandQueue* graphicsQueue = deviceQueues->GraphicsQueueHandle();

	std::array commandListsToExecute = {reinterpret_cast<ID3D12CommandList*>(commandList)};
	graphicsQueue->ExecuteCommandLists((UINT)commandListsToExecute.size(), commandListsToExecute.data());

	UINT64 waitFenceValue = deviceQueues->GraphicsFenceSignal();
	deviceQueues->GraphicsQueueCpuWait(waitFenceValue);

	//=====================================================================================================================

	VkCommandPool   transferCommandPool   = workerCommandBuffers->GetMainThreadTransferCommandPool(0);
	VkCommandBuffer transferCommandBuffer = workerCommandBuffers->GetMainThreadTransferCommandBuffer(0);

	VkCommandPool   graphicsCommandPool   = workerCommandBuffers->GetMainThreadGraphicsCommandPool(0);
	VkCommandBuffer graphicsCommandBuffer = workerCommandBuffers->GetMainThreadGraphicsCommandBuffer(0);

	VkPipelineStageFlags invalidateStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

	VkSemaphore transferToGraphicsSemaphore = VK_NULL_HANDLE;
	{
		VkSemaphoreCreateInfo semaphoreCreateInfo;
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphoreCreateInfo.pNext = nullptr;
		semaphoreCreateInfo.flags = 0;

		ThrowIfFailed(vkCreateSemaphore(mSceneToBuild->mDeviceRef, &semaphoreCreateInfo, nullptr, &transferToGraphicsSemaphore));
	}

	//Baking
	ThrowIfFailed(vkResetCommandPool(mSceneToBuild->mDeviceRef, transferCommandPool, 0));

	//TODO: mGPU?
	VkCommandBufferBeginInfo transferCmdBufferBeginInfo;
	transferCmdBufferBeginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	transferCmdBufferBeginInfo.pNext            = nullptr;
	transferCmdBufferBeginInfo.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	transferCmdBufferBeginInfo.pInheritanceInfo = nullptr;

	ThrowIfFailed(vkBeginCommandBuffer(transferCommandBuffer, &transferCmdBufferBeginInfo));

	std::array sceneBuffers           = {mSceneToBuild->mSceneVertexBuffer,   mSceneToBuild->mSceneIndexBuffer};
	std::array sceneBufferAccessMasks = {VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, VK_ACCESS_INDEX_READ_BIT};

	//TODO: multithread this!
	std::vector<VkImageMemoryBarrier> imageTransferBarriers(mSceneToBuild->mSceneTextures.size());
	for(size_t i = 0; i < mSceneToBuild->mSceneTextures.size(); i++)
	{
		imageTransferBarriers[i].sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageTransferBarriers[i].pNext                           = nullptr;
		imageTransferBarriers[i].srcAccessMask                   = 0;
		imageTransferBarriers[i].dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageTransferBarriers[i].oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
		imageTransferBarriers[i].newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageTransferBarriers[i].srcQueueFamilyIndex             = deviceQueues->GetTransferQueueFamilyIndex();
		imageTransferBarriers[i].dstQueueFamilyIndex             = deviceQueues->GetTransferQueueFamilyIndex();
		imageTransferBarriers[i].image                           = mSceneToBuild->mSceneTextures[i];
		imageTransferBarriers[i].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		imageTransferBarriers[i].subresourceRange.baseMipLevel   = 0;
		imageTransferBarriers[i].subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
		imageTransferBarriers[i].subresourceRange.baseArrayLayer = 0;
		imageTransferBarriers[i].subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;
	}

	std::array<VkBufferMemoryBarrier, sceneBuffers.size()> bufferTransferBarriers;
	for(size_t i = 0; i < sceneBuffers.size(); i++)
	{
		bufferTransferBarriers[i].sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		bufferTransferBarriers[i].pNext               = nullptr;
		bufferTransferBarriers[i].srcAccessMask       = 0;
		bufferTransferBarriers[i].dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
		bufferTransferBarriers[i].srcQueueFamilyIndex = deviceQueues->GetTransferQueueFamilyIndex();
		bufferTransferBarriers[i].dstQueueFamilyIndex = deviceQueues->GetTransferQueueFamilyIndex();
		bufferTransferBarriers[i].buffer              = sceneBuffers[i];
		bufferTransferBarriers[i].offset              = 0;
		bufferTransferBarriers[i].size                = VK_WHOLE_SIZE;
	}

	std::array<VkMemoryBarrier, 0> memoryTransferBarriers;
	vkCmdPipelineBarrier(transferCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
		                 (uint32_t)memoryTransferBarriers.size(), memoryTransferBarriers.data(),
		                 (uint32_t)bufferTransferBarriers.size(), bufferTransferBarriers.data(),
		                 (uint32_t)imageTransferBarriers.size(),  imageTransferBarriers.data());


	for(size_t i = 0; i < mSceneTextures.size(); i++)
	{
		vkCmdCopyBufferToImage(transferCommandBuffer, mIntermediateBuffer, mSceneToBuild->mSceneTextures[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)mSceneTextureCopyInfos[i].size(), mSceneTextureCopyInfos[i].data());
	}

	std::array sceneBufferDataOffsets = {mIntermediateBufferVertexDataOffset,                      mIntermediateBufferIndexDataOffset};
	std::array sceneBufferDataSizes   = {mVertexBufferData.size() * sizeof(RenderableSceneVertex), mIndexBufferData.size() * sizeof(RenderableSceneIndex)};
	for(size_t i = 0; i < sceneBuffers.size(); i++)
	{
		VkBufferCopy copyRegion;
		copyRegion.srcOffset = sceneBufferDataOffsets[i];
		copyRegion.dstOffset = 0;
		copyRegion.size      = sceneBufferDataSizes[i];

		std::array copyRegions = {copyRegion};
		vkCmdCopyBuffer(transferCommandBuffer, mIntermediateBuffer, sceneBuffers[i], (uint32_t)(copyRegions.size()), copyRegions.data());
	}

	std::vector<VkImageMemoryBarrier> releaseImageInvalidateBarriers(mSceneToBuild->mSceneTextures.size());
	for(size_t i = 0; i < mSceneToBuild->mSceneTextures.size(); i++)
	{
		releaseImageInvalidateBarriers[i].sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		releaseImageInvalidateBarriers[i].pNext                           = nullptr;
		releaseImageInvalidateBarriers[i].srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
		releaseImageInvalidateBarriers[i].dstAccessMask                   = 0;
		releaseImageInvalidateBarriers[i].oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		releaseImageInvalidateBarriers[i].newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		releaseImageInvalidateBarriers[i].srcQueueFamilyIndex             = deviceQueues->GetTransferQueueFamilyIndex();
		releaseImageInvalidateBarriers[i].dstQueueFamilyIndex             = deviceQueues->GetGraphicsQueueFamilyIndex();
		releaseImageInvalidateBarriers[i].image                           = mSceneToBuild->mSceneTextures[i];
		releaseImageInvalidateBarriers[i].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		releaseImageInvalidateBarriers[i].subresourceRange.baseMipLevel   = 0;
		releaseImageInvalidateBarriers[i].subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
		releaseImageInvalidateBarriers[i].subresourceRange.baseArrayLayer = 0;
		releaseImageInvalidateBarriers[i].subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;
	}

	std::array<VkBufferMemoryBarrier, sceneBuffers.size()> releaseBufferInvalidateBarriers;
	for(size_t i = 0; i < sceneBuffers.size(); i++)
	{
		releaseBufferInvalidateBarriers[i].sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		releaseBufferInvalidateBarriers[i].pNext               = nullptr;
		releaseBufferInvalidateBarriers[i].srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
		releaseBufferInvalidateBarriers[i].dstAccessMask       = 0;
		releaseBufferInvalidateBarriers[i].srcQueueFamilyIndex = deviceQueues->GetTransferQueueFamilyIndex();
		releaseBufferInvalidateBarriers[i].dstQueueFamilyIndex = deviceQueues->GetGraphicsQueueFamilyIndex();
		releaseBufferInvalidateBarriers[i].buffer              = sceneBuffers[i];
		releaseBufferInvalidateBarriers[i].offset              = 0;
		releaseBufferInvalidateBarriers[i].size                = VK_WHOLE_SIZE;
	}

	std::array<VkMemoryBarrier, 0>       memoryInvalidateBarriers;
	vkCmdPipelineBarrier(transferCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, invalidateStageMask, 0,
		                 (uint32_t)memoryInvalidateBarriers.size(),        memoryInvalidateBarriers.data(),
		                 (uint32_t)releaseBufferInvalidateBarriers.size(), releaseBufferInvalidateBarriers.data(),
		                 (uint32_t)releaseImageInvalidateBarriers.size(),  releaseImageInvalidateBarriers.data());

	ThrowIfFailed(vkEndCommandBuffer(transferCommandBuffer));

	deviceQueues->TransferQueueSubmit(transferCommandBuffer, transferToGraphicsSemaphore);

	//Graphics queue ownership
	ThrowIfFailed(vkResetCommandPool(mSceneToBuild->mDeviceRef, graphicsCommandPool, 0));

	//TODO: mGPU?
	VkCommandBufferBeginInfo graphicsCmdBufferBeginInfo;
	graphicsCmdBufferBeginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	graphicsCmdBufferBeginInfo.pNext            = nullptr;
	graphicsCmdBufferBeginInfo.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	graphicsCmdBufferBeginInfo.pInheritanceInfo = nullptr;

	ThrowIfFailed(vkBeginCommandBuffer(graphicsCommandBuffer, &graphicsCmdBufferBeginInfo));

	std::vector<VkImageMemoryBarrier> acquireImageInvalidateBarriers(mSceneToBuild->mSceneTextures.size());
	for(size_t i = 0; i < mSceneToBuild->mSceneTextures.size(); i++)
	{
		acquireImageInvalidateBarriers[i].sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		acquireImageInvalidateBarriers[i].pNext                           = nullptr;
		acquireImageInvalidateBarriers[i].srcAccessMask                   = 0;
		acquireImageInvalidateBarriers[i].dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;
		acquireImageInvalidateBarriers[i].oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		acquireImageInvalidateBarriers[i].newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		acquireImageInvalidateBarriers[i].srcQueueFamilyIndex             = deviceQueues->GetTransferQueueFamilyIndex();
		acquireImageInvalidateBarriers[i].dstQueueFamilyIndex             = deviceQueues->GetGraphicsQueueFamilyIndex();
		acquireImageInvalidateBarriers[i].image                           = mSceneToBuild->mSceneTextures[i];
		acquireImageInvalidateBarriers[i].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		acquireImageInvalidateBarriers[i].subresourceRange.baseMipLevel   = 0;
		acquireImageInvalidateBarriers[i].subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
		acquireImageInvalidateBarriers[i].subresourceRange.baseArrayLayer = 0;
		acquireImageInvalidateBarriers[i].subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;
	}

	std::array<VkBufferMemoryBarrier, sceneBuffers.size()> acquireBufferInvalidateBarriers;
	for(size_t i = 0; i < sceneBuffers.size(); i++)
	{
		acquireBufferInvalidateBarriers[i].sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		acquireBufferInvalidateBarriers[i].pNext               = nullptr;
		acquireBufferInvalidateBarriers[i].srcAccessMask       = 0;
		acquireBufferInvalidateBarriers[i].dstAccessMask       = sceneBufferAccessMasks[i];
		acquireBufferInvalidateBarriers[i].srcQueueFamilyIndex = deviceQueues->GetTransferQueueFamilyIndex();
		acquireBufferInvalidateBarriers[i].dstQueueFamilyIndex = deviceQueues->GetGraphicsQueueFamilyIndex();
		acquireBufferInvalidateBarriers[i].buffer              = sceneBuffers[i];
		acquireBufferInvalidateBarriers[i].offset              = 0;
		acquireBufferInvalidateBarriers[i].size                = VK_WHOLE_SIZE;
	}

	vkCmdPipelineBarrier(graphicsCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, invalidateStageMask, 0,
		                 (uint32_t)memoryInvalidateBarriers.size(),        memoryInvalidateBarriers.data(),
		                 (uint32_t)acquireBufferInvalidateBarriers.size(), acquireBufferInvalidateBarriers.data(),
		                 (uint32_t)acquireImageInvalidateBarriers.size(),  acquireImageInvalidateBarriers.data());

	ThrowIfFailed(vkEndCommandBuffer(graphicsCommandBuffer));

	deviceQueues->GraphicsQueueSubmit(graphicsCommandBuffer, transferToGraphicsSemaphore, VK_PIPELINE_STAGE_TRANSFER_BIT);
	deviceQueues->GraphicsQueueWait();

	SafeDestroyObject(vkDestroySemaphore, mSceneToBuild->mDeviceRef, transferToGraphicsSemaphore);
}

void ModernRenderableSceneBuilder::CreateSceneMeshMetadata(std::vector<std::wstring>& sceneTexturesVec)
{
	mVertexBufferData.clear();
	mIndexBufferData.clear();

	mSceneToBuild->mSceneMeshes.clear();
	mSceneToBuild->mSceneMeshes.clear();
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
	size_t intermediateBufferSize        = mIntermediateBufferTextureDataOffset;

	mTextureData.clear();

	AllocateTextureMetadataArrays(sceneTextures.size());
	for(size_t i = 0; i < sceneTextures.size(); i++)
	{
		LoadTextureFromFile(sceneTextures[i], i);
		intermediateBufferSize += ConstructTextureSubresources(intermediateBufferSize, i);
	}

	return intermediateBufferSize;
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
