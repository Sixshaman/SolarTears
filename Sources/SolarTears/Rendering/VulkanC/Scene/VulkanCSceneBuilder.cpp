#include "VulkanCSceneBuilder.hpp"
#include "../VulkanCUtils.hpp"
#include "../VulkanCFunctions.hpp"
#include "../VulkanCMemory.hpp"
#include "../VulkanCShaders.hpp"
#include "../VulkanCDeviceQueues.hpp"
#include "../VulkanCWorkerCommandBuffers.hpp"
#include "../../../../3rd party/DDSTextureLoaderVk/DDSTextureLoaderVk.h"
#include <array>
#include <cassert>

VulkanCBindings::RenderableSceneBuilder::RenderableSceneBuilder(RenderableScene* sceneToBuild): mSceneToBuild(sceneToBuild)
{
	assert(sceneToBuild != nullptr);

	mIntermediateBuffer        = VK_NULL_HANDLE;
	mIntermediateBufferMemory  = VK_NULL_HANDLE;

	mIntermediateBufferVertexDataOffset  = 0;
	mIntermediateBufferIndexDataOffset   = 0;
	mIntermediateBufferTextureDataOffset = 0;
}

VulkanCBindings::RenderableSceneBuilder::~RenderableSceneBuilder()
{
	SafeDestroyObject(vkDestroyBuffer, mSceneToBuild->mDeviceRef, mIntermediateBuffer);
	SafeDestroyObject(vkFreeMemory,    mSceneToBuild->mDeviceRef, mIntermediateBufferMemory);
}

void VulkanCBindings::RenderableSceneBuilder::BakeSceneFirstPart(const DeviceQueues* deviceQueues, const MemoryManager* memoryAllocator, const ShaderManager* shaderManager, const DeviceParameters& deviceParameters)
{
	//Create scene subobjects
	std::vector<std::wstring> sceneTexturesVec;
	CreateSceneMeshMetadata(sceneTexturesVec);

	VkDeviceSize intermediateBufferOffset = 0;

	//Create buffers for model and uniform data
	CreateSceneDataBuffers(deviceQueues, &intermediateBufferOffset);

	//Load texture images
	LoadTextureImages(sceneTexturesVec, deviceParameters, &intermediateBufferOffset);

	//Allocate memory for images and buffers
	AllocateImageMemory(memoryAllocator);
	AllocateBuffersMemory(memoryAllocator);

	CreateImageViews();

	//Create intermediate buffers
	VkDeviceSize currentIntermediateBufferSize = intermediateBufferOffset; //The size of current data is the offset to write new data, simple
	CreateIntermediateBuffer(deviceQueues, memoryAllocator, currentIntermediateBufferSize);

	//Fill intermediate buffers
	FillIntermediateBufferData();

	CreateDescriptorPool();
	AllocateDescriptorSets();
	FillDescriptorSets(shaderManager);
}

void VulkanCBindings::RenderableSceneBuilder::BakeSceneSecondPart(DeviceQueues* deviceQueues, WorkerCommandBuffers* workerCommandBuffers)
{
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


	for(size_t i = 0; i < mSceneToBuild->mSceneTextures.size(); i++)
	{
		vkCmdCopyBufferToImage(transferCommandBuffer, mIntermediateBuffer, mSceneToBuild->mSceneTextures[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t)mSceneTextureSubresourses[i].size(), mSceneTextureSubresourses[i].data());
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

void VulkanCBindings::RenderableSceneBuilder::CreateSceneMeshMetadata(std::vector<std::wstring>& sceneTexturesVec)
{
	mVertexBufferData.clear();
	mIndexBufferData.clear();

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
		RenderableScene::MeshSubobjectRange subobjectRange;
		subobjectRange.FirstSubobjectIndex     = (uint32_t)i;
		subobjectRange.AfterLastSubobjectIndex = (uint32_t)(i + 1);

		mSceneToBuild->mSceneMeshes.push_back(subobjectRange);

		RenderableScene::SceneSubobject subobject;
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
	mSceneToBuild->mObjectDataScheduledUpdateIndices.resize(mSceneToBuild->mSceneSubobjects.size());
}

void VulkanCBindings::RenderableSceneBuilder::CreateSceneDataBuffers(const DeviceQueues* deviceQueues, VkDeviceSize* currentIntermediateBufferOffset)
{
	SafeDestroyObject(vkDestroyBuffer, mSceneToBuild->mDeviceRef, mSceneToBuild->mSceneVertexBuffer);
	SafeDestroyObject(vkDestroyBuffer, mSceneToBuild->mDeviceRef, mSceneToBuild->mSceneIndexBuffer);
	SafeDestroyObject(vkDestroyBuffer, mSceneToBuild->mDeviceRef, mSceneToBuild->mSceneUniformBuffer);
		
	std::array bufferQueueFamilies = {deviceQueues->GetGraphicsQueueFamilyIndex()};

	const VkDeviceSize vertexDataSize = mVertexBufferData.size() * sizeof(RenderableSceneVertex);

	//TODO: buffer device address
	//TODO: dedicated allocation
	VkBufferCreateInfo vertexBufferCreateInfo;
	vertexBufferCreateInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertexBufferCreateInfo.pNext                 = nullptr;
	vertexBufferCreateInfo.flags                 = 0;
	vertexBufferCreateInfo.size                  = vertexDataSize;
	vertexBufferCreateInfo.usage                 = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	vertexBufferCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
	vertexBufferCreateInfo.queueFamilyIndexCount = (uint32_t)bufferQueueFamilies.size();
	vertexBufferCreateInfo.pQueueFamilyIndices   = bufferQueueFamilies.data();

	ThrowIfFailed(vkCreateBuffer(mSceneToBuild->mDeviceRef, &vertexBufferCreateInfo, nullptr, &mSceneToBuild->mSceneVertexBuffer));

	mIntermediateBufferVertexDataOffset = *currentIntermediateBufferOffset;
	*currentIntermediateBufferOffset    = mIntermediateBufferVertexDataOffset + vertexDataSize;


	const VkDeviceSize indexDataSize = mIndexBufferData.size() * sizeof(RenderableSceneIndex);

	//TODO: buffer device address
	//TODO: dedicated allocation
	VkBufferCreateInfo indexBufferCreateInfo;
	indexBufferCreateInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	indexBufferCreateInfo.pNext                 = nullptr;
	indexBufferCreateInfo.flags                 = 0;
	indexBufferCreateInfo.size                  = indexDataSize;
	indexBufferCreateInfo.usage                 = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	indexBufferCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
	indexBufferCreateInfo.queueFamilyIndexCount = (uint32_t)bufferQueueFamilies.size();
	indexBufferCreateInfo.pQueueFamilyIndices   = bufferQueueFamilies.data();

	ThrowIfFailed(vkCreateBuffer(mSceneToBuild->mDeviceRef, &indexBufferCreateInfo, nullptr, &mSceneToBuild->mSceneIndexBuffer));

	mIntermediateBufferIndexDataOffset = *currentIntermediateBufferOffset;
	*currentIntermediateBufferOffset   = mIntermediateBufferIndexDataOffset + indexDataSize;


	VkDeviceSize uniformPerObjectDataSize = mSceneToBuild->mScenePerObjectData.size() * mSceneToBuild->mGBufferObjectChunkDataSize * VulkanUtils::InFlightFrameCount;
	VkDeviceSize uniformPerFrameDataSize  = mSceneToBuild->mGBufferFrameChunkDataSize * VulkanUtils::InFlightFrameCount;

	//TODO: buffer device address
	//TODO: dedicated allocation
	VkBufferCreateInfo uniformBufferCreateInfo;
	uniformBufferCreateInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	uniformBufferCreateInfo.pNext                 = nullptr;
	uniformBufferCreateInfo.flags                 = 0;
	uniformBufferCreateInfo.size                  = uniformPerObjectDataSize + uniformPerFrameDataSize;
	uniformBufferCreateInfo.usage                 = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	uniformBufferCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
	uniformBufferCreateInfo.queueFamilyIndexCount = (uint32_t)bufferQueueFamilies.size();
	uniformBufferCreateInfo.pQueueFamilyIndices   = bufferQueueFamilies.data();

	ThrowIfFailed(vkCreateBuffer(mSceneToBuild->mDeviceRef, &uniformBufferCreateInfo, nullptr, &mSceneToBuild->mSceneUniformBuffer));

	mSceneToBuild->mSceneDataUniformObjectBufferOffset = 0;
	mSceneToBuild->mSceneDataUniformFrameBufferOffset  = mSceneToBuild->mSceneDataUniformObjectBufferOffset + uniformPerObjectDataSize;
}

void VulkanCBindings::RenderableSceneBuilder::LoadTextureImages(const std::vector<std::wstring>& sceneTextures, const DeviceParameters& deviceParameters, VkDeviceSize* currentIntermediateBufferOffset)
{
	mTextureData.clear();

	VkDeviceSize currentOffset = *currentIntermediateBufferOffset;
	for(size_t i = 0; i < sceneTextures.size(); i++)
	{
		std::vector<uint8_t>           texData;
		std::vector<VkBufferImageCopy> texSubresources;

		VkImage texture;
		ThrowIfFailed(Vulkan::LoadDDSTextureFromFile(mSceneToBuild->mDeviceRef, sceneTextures[i].c_str(), &texture, texData, texSubresources, deviceParameters.GetDeviceProperties().limits.maxImageDimension2D, currentOffset));

		mTextureData.insert(mTextureData.end(), texData.begin(), texData.end());
		mSceneToBuild->mSceneTextures.push_back(texture);
		mSceneTextureSubresourses.push_back(texSubresources);

		currentOffset += texData.size() * sizeof(uint8_t);
	}

	mIntermediateBufferTextureDataOffset = *currentIntermediateBufferOffset;
	*currentIntermediateBufferOffset     = *currentIntermediateBufferOffset + mTextureData.size() * sizeof(uint8_t);
}

void VulkanCBindings::RenderableSceneBuilder::AllocateImageMemory(const MemoryManager* memoryAllocator)
{
	SafeDestroyObject(vkFreeMemory, mSceneToBuild->mDeviceRef, mSceneToBuild->mTextureMemory);

	std::vector<VkDeviceSize> textureMemoryOffsets;
	mSceneToBuild->mTextureMemory = memoryAllocator->AllocateImagesMemory(mSceneToBuild->mDeviceRef, mSceneToBuild->mSceneTextures, textureMemoryOffsets);

	std::vector<VkBindImageMemoryInfo> bindImageMemoryInfos(mSceneToBuild->mSceneTextures.size());
	for(size_t i = 0; i < mSceneToBuild->mSceneTextures.size(); i++)
	{
		//TODO: mGPU?
		bindImageMemoryInfos[i].sType        = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
		bindImageMemoryInfos[i].pNext        = nullptr;
		bindImageMemoryInfos[i].memory       = mSceneToBuild->mTextureMemory;
		bindImageMemoryInfos[i].memoryOffset = textureMemoryOffsets[i];
		bindImageMemoryInfos[i].image        = mSceneToBuild->mSceneTextures[i];
	}

	ThrowIfFailed(vkBindImageMemory2(mSceneToBuild->mDeviceRef, (uint32_t)(bindImageMemoryInfos.size()), bindImageMemoryInfos.data()));
}

void VulkanCBindings::RenderableSceneBuilder::AllocateBuffersMemory(const MemoryManager* memoryAllocator)
{
	SafeDestroyObject(vkFreeMemory, mSceneToBuild->mDeviceRef, mSceneToBuild->mBufferMemory);
	SafeDestroyObject(vkFreeMemory, mSceneToBuild->mDeviceRef, mSceneToBuild->mBufferHostVisibleMemory);


	std::vector deviceLocalBuffers = {mSceneToBuild->mSceneVertexBuffer, mSceneToBuild->mSceneIndexBuffer};

	std::vector<VkDeviceSize> deviceLocalBufferOffsets;
	mSceneToBuild->mBufferMemory = memoryAllocator->AllocateBuffersMemory(mSceneToBuild->mDeviceRef, deviceLocalBuffers, MemoryManager::BufferAllocationType::DEVICE_LOCAL, deviceLocalBufferOffsets);

	std::vector<VkBindBufferMemoryInfo> bindBufferMemoryInfos(deviceLocalBuffers.size());
	for(size_t i = 0; i < deviceLocalBuffers.size(); i++)
	{
		//TODO: mGPU?
		bindBufferMemoryInfos[i].sType        = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO;
		bindBufferMemoryInfos[i].pNext        = nullptr;
		bindBufferMemoryInfos[i].buffer       = deviceLocalBuffers[i];
		bindBufferMemoryInfos[i].memory       = mSceneToBuild->mBufferMemory;
		bindBufferMemoryInfos[i].memoryOffset = deviceLocalBufferOffsets[i];
	}

	ThrowIfFailed(vkBindBufferMemory2(mSceneToBuild->mDeviceRef, (uint32_t)(bindBufferMemoryInfos.size()), bindBufferMemoryInfos.data()));


	std::vector hostVisibleBuffers = {mSceneToBuild->mSceneUniformBuffer};

	std::vector<VkDeviceSize> hostVisibleBufferOffsets;
	mSceneToBuild->mBufferHostVisibleMemory = memoryAllocator->AllocateBuffersMemory(mSceneToBuild->mDeviceRef, hostVisibleBuffers, MemoryManager::BufferAllocationType::HOST_VISIBLE, hostVisibleBufferOffsets);

	std::vector<VkBindBufferMemoryInfo> bindHostVisibleBufferMemoryInfos(hostVisibleBuffers.size());
	for(size_t i = 0; i < hostVisibleBuffers.size(); i++)
	{
		//TODO: mGPU?
		bindHostVisibleBufferMemoryInfos[i].sType        = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO;
		bindHostVisibleBufferMemoryInfos[i].pNext        = nullptr;
		bindHostVisibleBufferMemoryInfos[i].buffer       = hostVisibleBuffers[i];
		bindHostVisibleBufferMemoryInfos[i].memory       = mSceneToBuild->mBufferHostVisibleMemory;
		bindHostVisibleBufferMemoryInfos[i].memoryOffset = hostVisibleBufferOffsets[i];
	}

	ThrowIfFailed(vkBindBufferMemory2(mSceneToBuild->mDeviceRef, (uint32_t)(bindHostVisibleBufferMemoryInfos.size()), bindHostVisibleBufferMemoryInfos.data()));

	ThrowIfFailed(vkMapMemory(mSceneToBuild->mDeviceRef, mSceneToBuild->mBufferHostVisibleMemory, 0, VK_WHOLE_SIZE, 0, &mSceneToBuild->mSceneUniformDataBufferPointer));
}

void VulkanCBindings::RenderableSceneBuilder::CreateImageViews()
{
	for(size_t i = 0; i < mSceneToBuild->mSceneTextures.size(); i++)
	{
		VkImageViewCreateInfo imageViewCreateInfo;
		imageViewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext                           = nullptr;
		imageViewCreateInfo.flags                           = 0;
		imageViewCreateInfo.image                           = mSceneToBuild->mSceneTextures[i];
		imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D; //Only 2D-textures are stored in mSceneTextures
		imageViewCreateInfo.format                          = VK_FORMAT_B8G8R8A8_UNORM;
		imageViewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
		imageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
		imageViewCreateInfo.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

		VkImageView imageView = VK_NULL_HANDLE;
		ThrowIfFailed(vkCreateImageView(mSceneToBuild->mDeviceRef, &imageViewCreateInfo, nullptr, &imageView));

		mSceneToBuild->mSceneTextureViews.push_back(imageView);
	}
}

void VulkanCBindings::RenderableSceneBuilder::CreateDescriptorPool() //œ”Àﬂ ƒ≈— –»œ“Œ–Œ¬! œ¿”!
{	
	std::unordered_map<VkDescriptorType, uint32_t> descriptorTypeCounts;

	descriptorTypeCounts[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER]         = 0;
	descriptorTypeCounts[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC] = 0;
	descriptorTypeCounts[VK_DESCRIPTOR_TYPE_SAMPLER]                = 0;
	descriptorTypeCounts[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] = 0;
	descriptorTypeCounts[VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE]          = 0;

	descriptorTypeCounts.at(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) += (uint32_t)(mSceneToBuild->mSceneTextures.size());
	descriptorTypeCounts.at(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) += 2; //Per-frame and per-object uniform buffers

	uint32_t maxDescriptorSets = 0;
	std::vector<VkDescriptorPoolSize> descriptorPoolSizes;
	for(auto it = descriptorTypeCounts.begin(); it != descriptorTypeCounts.end(); it++)
	{
		if(it->second != 0)
		{
			VkDescriptorPoolSize descriptorPoolSize;
			descriptorPoolSize.type            = it->first;
			descriptorPoolSize.descriptorCount = it->second;
			descriptorPoolSizes.push_back(descriptorPoolSize);

			maxDescriptorSets += it->second;
		}
	}

	//TODO: inline uniforms
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
	descriptorPoolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext         = nullptr;
	descriptorPoolCreateInfo.flags         = 0;
	descriptorPoolCreateInfo.maxSets       = maxDescriptorSets;
	descriptorPoolCreateInfo.poolSizeCount = (uint32_t)(descriptorPoolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes    = descriptorPoolSizes.data();

	ThrowIfFailed(vkCreateDescriptorPool(mSceneToBuild->mDeviceRef, &descriptorPoolCreateInfo, nullptr, &mSceneToBuild->mDescriptorPool));
}

void VulkanCBindings::RenderableSceneBuilder::AllocateDescriptorSets()
{
	std::array bufferDescriptorSetLayouts = {mSceneToBuild->mGBufferUniformsDescriptorSetLayout};

	//TODO: variable descriptor count
	VkDescriptorSetAllocateInfo bufferDescriptorSetAllocateInfo;
	bufferDescriptorSetAllocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	bufferDescriptorSetAllocateInfo.pNext              = nullptr;
	bufferDescriptorSetAllocateInfo.descriptorPool     = mSceneToBuild->mDescriptorPool;
	bufferDescriptorSetAllocateInfo.descriptorSetCount = (uint32_t)(bufferDescriptorSetLayouts.size());
	bufferDescriptorSetAllocateInfo.pSetLayouts        = bufferDescriptorSetLayouts.data();

	std::array bufferDescriptorSets = {mSceneToBuild->mGBufferUniformsDescriptorSet};
	ThrowIfFailed(vkAllocateDescriptorSets(mSceneToBuild->mDeviceRef, &bufferDescriptorSetAllocateInfo, bufferDescriptorSets.data()));
	mSceneToBuild->mGBufferUniformsDescriptorSet = bufferDescriptorSets[0];


	std::vector<VkDescriptorSetLayout> textureDescriptorSetLayouts(mSceneToBuild->mSceneTextures.size());
	for(size_t i = 0; i < mSceneToBuild->mSceneTextures.size(); i++)
	{
		textureDescriptorSetLayouts[i] = mSceneToBuild->mGBufferTexturesDescriptorSetLayout;
	}

	//TODO: variable descriptor count
	VkDescriptorSetAllocateInfo textureDescriptorSetAllocateInfo;
	textureDescriptorSetAllocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	textureDescriptorSetAllocateInfo.pNext              = nullptr;
	textureDescriptorSetAllocateInfo.descriptorPool     = mSceneToBuild->mDescriptorPool;
	textureDescriptorSetAllocateInfo.descriptorSetCount = (uint32_t)(textureDescriptorSetLayouts.size());
	textureDescriptorSetAllocateInfo.pSetLayouts        = textureDescriptorSetLayouts.data();

	mSceneToBuild->mGBufferTextureDescriptorSets.resize(mSceneToBuild->mSceneTextures.size());
	ThrowIfFailed(vkAllocateDescriptorSets(mSceneToBuild->mDeviceRef, &textureDescriptorSetAllocateInfo, mSceneToBuild->mGBufferTextureDescriptorSets.data()));
}

void VulkanCBindings::RenderableSceneBuilder::FillDescriptorSets(const ShaderManager* shaderManager)
{
	std::vector<VkWriteDescriptorSet> writeDescriptorSets;
	std::vector<VkCopyDescriptorSet>  copyDescriptorSets;

	VkDeviceSize uniformObjectDataSize = mSceneToBuild->mScenePerObjectData.size() * mSceneToBuild->mGBufferObjectChunkDataSize;
	VkDeviceSize uniformFrameDataSize  = mSceneToBuild->mGBufferFrameChunkDataSize;

	std::array gbufferUniformBindingNames = {"UniformBufferObject",                              "UniformBufferFrame"};
	std::array gbufferUniformOffsets      = {mSceneToBuild->mSceneDataUniformObjectBufferOffset, mSceneToBuild->mSceneDataUniformFrameBufferOffset};
	std::array gbufferUniformRanges       = {uniformObjectDataSize,                              uniformFrameDataSize};

	uint32_t minUniformBindingNumber = UINT32_MAX; //Uniform buffer descriptor set has sequentional bindings

	std::array<VkDescriptorBufferInfo, gbufferUniformBindingNames.size()> gbufferDescriptorBufferInfos;
	for(size_t i = 0; i < gbufferUniformBindingNames.size(); i++)
	{
		uint32_t bindingNumber = 0;
		shaderManager->GetGBufferDrawDescriptorBindingInfo(gbufferUniformBindingNames[i], &bindingNumber, nullptr, nullptr, nullptr, nullptr);

		minUniformBindingNumber = std::min(minUniformBindingNumber, bindingNumber);

		gbufferDescriptorBufferInfos[i].buffer = mSceneToBuild->mSceneUniformBuffer;
		gbufferDescriptorBufferInfos[i].offset = gbufferUniformOffsets[i];
		gbufferDescriptorBufferInfos[i].range  = gbufferUniformRanges[i];
	}

	//TODO: inline uniforms (and raytracing!)
	VkWriteDescriptorSet uniformWriteDescriptor;
	uniformWriteDescriptor.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	uniformWriteDescriptor.pNext            = nullptr;
	uniformWriteDescriptor.dstSet           = mSceneToBuild->mGBufferUniformsDescriptorSet;
	uniformWriteDescriptor.dstBinding       = minUniformBindingNumber; //Uniform buffer descriptor set is sequentional
	uniformWriteDescriptor.dstArrayElement  = 0; //Uniform buffers have only 1 array element
	uniformWriteDescriptor.descriptorCount  = (uint32_t)gbufferDescriptorBufferInfos.size();
	uniformWriteDescriptor.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	uniformWriteDescriptor.pImageInfo       = nullptr;
	uniformWriteDescriptor.pBufferInfo      = gbufferDescriptorBufferInfos.data();
	uniformWriteDescriptor.pTexelBufferView = nullptr;

	writeDescriptorSets.push_back(uniformWriteDescriptor);

	std::vector<VkDescriptorImageInfo> descriptorImageInfos;
	for(size_t i = 0; i < mSceneToBuild->mSceneTextures.size(); i++)
	{
		uint32_t         bindingNumber  = 0;
		uint32_t         arraySize      = 0;
		VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
		shaderManager->GetGBufferDrawDescriptorBindingInfo("ObjectTexture", &bindingNumber, nullptr, &arraySize, nullptr, &descriptorType);

		assert(arraySize == 1); //For now only 1 descriptor is supported

		VkDescriptorImageInfo descriptorImageInfo;
		descriptorImageInfo.imageView   = mSceneToBuild->mSceneTextureViews[i];
		descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		descriptorImageInfo.sampler     = nullptr; //Immutable samplers are used
		descriptorImageInfos.push_back(descriptorImageInfo);

		//TODO: inline uniforms (and raytracing!)
		VkWriteDescriptorSet textureWriteDescriptor;
		textureWriteDescriptor.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		textureWriteDescriptor.pNext            = nullptr;
		textureWriteDescriptor.dstSet           = mSceneToBuild->mGBufferTextureDescriptorSets[i];
		textureWriteDescriptor.dstBinding       = bindingNumber;
		textureWriteDescriptor.dstArrayElement  = 0;
		textureWriteDescriptor.descriptorCount  = arraySize;
		textureWriteDescriptor.descriptorType   = descriptorType;
		textureWriteDescriptor.pImageInfo       = &descriptorImageInfos.back();
		textureWriteDescriptor.pBufferInfo      = nullptr;
		textureWriteDescriptor.pTexelBufferView = nullptr;

		writeDescriptorSets.push_back(textureWriteDescriptor);
	}

	vkUpdateDescriptorSets(mSceneToBuild->mDeviceRef, (uint32_t)(writeDescriptorSets.size()), writeDescriptorSets.data(), (uint32_t)(copyDescriptorSets.size()), copyDescriptorSets.data());
}

void VulkanCBindings::RenderableSceneBuilder::CreateIntermediateBuffer(const DeviceQueues* deviceQueues, const MemoryManager* memoryAllocator, VkDeviceSize intermediateBufferSize)
{
	std::array intermediateBufferQueues = {deviceQueues->GetTransferQueueFamilyIndex()};

	VkBufferCreateInfo intermediateBufferCreateInfo;
	intermediateBufferCreateInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	intermediateBufferCreateInfo.pNext                 = nullptr;
	intermediateBufferCreateInfo.flags                 = 0;
	intermediateBufferCreateInfo.size                  = intermediateBufferSize;
	intermediateBufferCreateInfo.usage                 = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	intermediateBufferCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
	intermediateBufferCreateInfo.queueFamilyIndexCount = (uint32_t)intermediateBufferQueues.size();
	intermediateBufferCreateInfo.pQueueFamilyIndices   = intermediateBufferQueues.data();

	vkCreateBuffer(mSceneToBuild->mDeviceRef, &intermediateBufferCreateInfo, nullptr, &mIntermediateBuffer);

	mIntermediateBufferMemory = memoryAllocator->AllocateIntermediateBufferMemory(mSceneToBuild->mDeviceRef, mIntermediateBuffer);

	//TODO: mGPU?
	VkBindBufferMemoryInfo bindBufferMemoryInfo;
	bindBufferMemoryInfo.sType        = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO;
	bindBufferMemoryInfo.pNext        = nullptr;
	bindBufferMemoryInfo.buffer       = mIntermediateBuffer;
	bindBufferMemoryInfo.memory       = mIntermediateBufferMemory;
	bindBufferMemoryInfo.memoryOffset = 0;
	
	std::array bindInfos = {bindBufferMemoryInfo};
	ThrowIfFailed(vkBindBufferMemory2(mSceneToBuild->mDeviceRef, (uint32_t)(bindInfos.size()), bindInfos.data()));
}

void VulkanCBindings::RenderableSceneBuilder::FillIntermediateBufferData()
{
	const VkDeviceSize textureDataSize = mTextureData.size()      * sizeof(uint8_t);
	const VkDeviceSize vertexDataSize  = mVertexBufferData.size() * sizeof(RenderableSceneVertex);
	const VkDeviceSize indexDataSize   = mIndexBufferData.size()  * sizeof(RenderableSceneIndex);

	void* bufferData = nullptr;
	ThrowIfFailed(vkMapMemory(mSceneToBuild->mDeviceRef, mIntermediateBufferMemory, 0, VK_WHOLE_SIZE, 0, &bufferData));

	std::byte* bufferDataBytes = reinterpret_cast<std::byte*>(bufferData);
	memcpy(bufferDataBytes + mIntermediateBufferVertexDataOffset,  mVertexBufferData.data(), vertexDataSize);
	memcpy(bufferDataBytes + mIntermediateBufferIndexDataOffset,   mIndexBufferData.data(),  indexDataSize);
	memcpy(bufferDataBytes + mIntermediateBufferTextureDataOffset, mTextureData.data(),      textureDataSize);

	VkMappedMemoryRange mappedRange;
	mappedRange.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	mappedRange.pNext  = nullptr;
	mappedRange.memory = mIntermediateBufferMemory;
	mappedRange.offset = 0;
	mappedRange.size   = VK_WHOLE_SIZE;

	std::array mappedRanges = {mappedRange};
	ThrowIfFailed(vkFlushMappedMemoryRanges(mSceneToBuild->mDeviceRef, (uint32_t)(mappedRanges.size()), mappedRanges.data()));

	vkUnmapMemory(mSceneToBuild->mDeviceRef, mIntermediateBufferMemory);
}