#include "VulkanSceneBuilder.hpp"
#include "../VulkanUtils.hpp"
#include "../VulkanFunctions.hpp"
#include "../VulkanMemory.hpp"
#include "../VulkanShaders.hpp"
#include "../VulkanDescriptorManager.hpp"
#include "../VulkanDeviceQueues.hpp"
#include "../VulkanWorkerCommandBuffers.hpp"
#include "../../Common/RenderingUtils.hpp"
#include "../../../../3rdParty/DDSTextureLoaderVk/DDSTextureLoaderVk.h"
#include <array>
#include <cassert>

Vulkan::RenderableSceneBuilder::RenderableSceneBuilder(RenderableScene* sceneToBuild, MemoryManager* memoryAllocator, DeviceQueues* deviceQueues, 
	                                                   WorkerCommandBuffers* workerCommandBuffers, const DeviceParameters* deviceParameters): ModernRenderableSceneBuilder(sceneToBuild), mVulkanSceneToBuild(sceneToBuild), mMemoryAllocator(memoryAllocator),
                                                                                                                                              mDeviceQueues(deviceQueues), mWorkerCommandBuffers(workerCommandBuffers), mDeviceParametersRef(deviceParameters)
{
	assert(sceneToBuild != nullptr);

	mIntermediateBuffer        = VK_NULL_HANDLE;
	mIntermediateBufferMemory  = VK_NULL_HANDLE;

	DDSTextureLoaderVk::SetVkCreateImageFuncPtr(vkCreateImage);

	mTexturePlacementAlignment = 1;
}

Vulkan::RenderableSceneBuilder::~RenderableSceneBuilder()
{
	SafeDestroyObject(vkDestroyBuffer, mVulkanSceneToBuild->mDeviceRef, mIntermediateBuffer);
	SafeDestroyObject(vkFreeMemory,    mVulkanSceneToBuild->mDeviceRef, mIntermediateBufferMemory);
}

void Vulkan::RenderableSceneBuilder::BakeSceneSecondPart()
{
	VkCommandPool   transferCommandPool   = mWorkerCommandBuffers->GetMainThreadTransferCommandPool(0);
	VkCommandBuffer transferCommandBuffer = mWorkerCommandBuffers->GetMainThreadTransferCommandBuffer(0);

	VkCommandPool   graphicsCommandPool   = mWorkerCommandBuffers->GetMainThreadGraphicsCommandPool(0);
	VkCommandBuffer graphicsCommandBuffer = mWorkerCommandBuffers->GetMainThreadGraphicsCommandBuffer(0);

	VkPipelineStageFlags invalidateStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

	VkSemaphore transferToGraphicsSemaphore = VK_NULL_HANDLE;
	{
		VkSemaphoreCreateInfo semaphoreCreateInfo;
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphoreCreateInfo.pNext = nullptr;
		semaphoreCreateInfo.flags = 0;

		ThrowIfFailed(vkCreateSemaphore(mVulkanSceneToBuild->mDeviceRef, &semaphoreCreateInfo, nullptr, &transferToGraphicsSemaphore));
	}

	//Baking
	ThrowIfFailed(vkResetCommandPool(mVulkanSceneToBuild->mDeviceRef, transferCommandPool, 0));

	//TODO: mGPU?
	VkCommandBufferBeginInfo transferCmdBufferBeginInfo;
	transferCmdBufferBeginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	transferCmdBufferBeginInfo.pNext            = nullptr;
	transferCmdBufferBeginInfo.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	transferCmdBufferBeginInfo.pInheritanceInfo = nullptr;

	ThrowIfFailed(vkBeginCommandBuffer(transferCommandBuffer, &transferCmdBufferBeginInfo));

	std::array sceneBuffers           = {mVulkanSceneToBuild->mSceneVertexBuffer, mVulkanSceneToBuild->mSceneIndexBuffer};
	std::array sceneBufferAccessMasks = {VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,     VK_ACCESS_INDEX_READ_BIT};

	//TODO: multithread this!
	std::vector<VkImageMemoryBarrier> imageTransferBarriers(mVulkanSceneToBuild->mSceneTextures.size());
	for(size_t i = 0; i < mVulkanSceneToBuild->mSceneTextures.size(); i++)
	{
		imageTransferBarriers[i].sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageTransferBarriers[i].pNext                           = nullptr;
		imageTransferBarriers[i].srcAccessMask                   = 0;
		imageTransferBarriers[i].dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageTransferBarriers[i].oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
		imageTransferBarriers[i].newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageTransferBarriers[i].srcQueueFamilyIndex             = mDeviceQueues->GetTransferQueueFamilyIndex();
		imageTransferBarriers[i].dstQueueFamilyIndex             = mDeviceQueues->GetTransferQueueFamilyIndex();
		imageTransferBarriers[i].image                           = mVulkanSceneToBuild->mSceneTextures[i];
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
		bufferTransferBarriers[i].srcQueueFamilyIndex = mDeviceQueues->GetTransferQueueFamilyIndex();
		bufferTransferBarriers[i].dstQueueFamilyIndex = mDeviceQueues->GetTransferQueueFamilyIndex();
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
		SubresourceArraySlice subresourceSlice = mSceneTextureSubresourceSlices[i];
		vkCmdCopyBufferToImage(transferCommandBuffer, mIntermediateBuffer, mVulkanSceneToBuild->mSceneTextures[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceSlice.End - subresourceSlice.Begin, mSceneImageCopyInfos.data() + subresourceSlice.Begin);
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

	std::vector<VkImageMemoryBarrier> releaseImageInvalidateBarriers(mVulkanSceneToBuild->mSceneTextures.size());
	for(size_t i = 0; i < mVulkanSceneToBuild->mSceneTextures.size(); i++)
	{
		releaseImageInvalidateBarriers[i].sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		releaseImageInvalidateBarriers[i].pNext                           = nullptr;
		releaseImageInvalidateBarriers[i].srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
		releaseImageInvalidateBarriers[i].dstAccessMask                   = 0;
		releaseImageInvalidateBarriers[i].oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		releaseImageInvalidateBarriers[i].newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		releaseImageInvalidateBarriers[i].srcQueueFamilyIndex             = mDeviceQueues->GetTransferQueueFamilyIndex();
		releaseImageInvalidateBarriers[i].dstQueueFamilyIndex             = mDeviceQueues->GetGraphicsQueueFamilyIndex();
		releaseImageInvalidateBarriers[i].image                           = mVulkanSceneToBuild->mSceneTextures[i];
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
		releaseBufferInvalidateBarriers[i].srcQueueFamilyIndex = mDeviceQueues->GetTransferQueueFamilyIndex();
		releaseBufferInvalidateBarriers[i].dstQueueFamilyIndex = mDeviceQueues->GetGraphicsQueueFamilyIndex();
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

	mDeviceQueues->TransferQueueSubmit(transferCommandBuffer, transferToGraphicsSemaphore);

	//Graphics queue ownership
	ThrowIfFailed(vkResetCommandPool(mVulkanSceneToBuild->mDeviceRef, graphicsCommandPool, 0));

	//TODO: mGPU?
	VkCommandBufferBeginInfo graphicsCmdBufferBeginInfo;
	graphicsCmdBufferBeginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	graphicsCmdBufferBeginInfo.pNext            = nullptr;
	graphicsCmdBufferBeginInfo.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	graphicsCmdBufferBeginInfo.pInheritanceInfo = nullptr;

	ThrowIfFailed(vkBeginCommandBuffer(graphicsCommandBuffer, &graphicsCmdBufferBeginInfo));

	std::vector<VkImageMemoryBarrier> acquireImageInvalidateBarriers(mVulkanSceneToBuild->mSceneTextures.size());
	for(size_t i = 0; i < mVulkanSceneToBuild->mSceneTextures.size(); i++)
	{
		acquireImageInvalidateBarriers[i].sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		acquireImageInvalidateBarriers[i].pNext                           = nullptr;
		acquireImageInvalidateBarriers[i].srcAccessMask                   = 0;
		acquireImageInvalidateBarriers[i].dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;
		acquireImageInvalidateBarriers[i].oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		acquireImageInvalidateBarriers[i].newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		acquireImageInvalidateBarriers[i].srcQueueFamilyIndex             = mDeviceQueues->GetTransferQueueFamilyIndex();
		acquireImageInvalidateBarriers[i].dstQueueFamilyIndex             = mDeviceQueues->GetGraphicsQueueFamilyIndex();
		acquireImageInvalidateBarriers[i].image                           = mVulkanSceneToBuild->mSceneTextures[i];
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
		acquireBufferInvalidateBarriers[i].srcQueueFamilyIndex = mDeviceQueues->GetTransferQueueFamilyIndex();
		acquireBufferInvalidateBarriers[i].dstQueueFamilyIndex = mDeviceQueues->GetGraphicsQueueFamilyIndex();
		acquireBufferInvalidateBarriers[i].buffer              = sceneBuffers[i];
		acquireBufferInvalidateBarriers[i].offset              = 0;
		acquireBufferInvalidateBarriers[i].size                = VK_WHOLE_SIZE;
	}

	vkCmdPipelineBarrier(graphicsCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, invalidateStageMask, 0,
		                 (uint32_t)memoryInvalidateBarriers.size(),        memoryInvalidateBarriers.data(),
		                 (uint32_t)acquireBufferInvalidateBarriers.size(), acquireBufferInvalidateBarriers.data(),
		                 (uint32_t)acquireImageInvalidateBarriers.size(),  acquireImageInvalidateBarriers.data());

	ThrowIfFailed(vkEndCommandBuffer(graphicsCommandBuffer));

	mDeviceQueues->GraphicsQueueSubmit(graphicsCommandBuffer, transferToGraphicsSemaphore, VK_PIPELINE_STAGE_TRANSFER_BIT);
	mDeviceQueues->GraphicsQueueWait();

	SafeDestroyObject(vkDestroySemaphore, mVulkanSceneToBuild->mDeviceRef, transferToGraphicsSemaphore);
}

void Vulkan::RenderableSceneBuilder::PreCreateVertexBuffer(size_t vertexDataSize)
{
	SafeDestroyObject(vkDestroyBuffer, mVulkanSceneToBuild->mDeviceRef, mVulkanSceneToBuild->mSceneVertexBuffer);

	std::array bufferQueueFamilies = {mDeviceQueues->GetGraphicsQueueFamilyIndex()};

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

	ThrowIfFailed(vkCreateBuffer(mVulkanSceneToBuild->mDeviceRef, &vertexBufferCreateInfo, nullptr, &mVulkanSceneToBuild->mSceneVertexBuffer));
}

void Vulkan::RenderableSceneBuilder::PreCreateIndexBuffer(size_t indexDataSize)
{
	SafeDestroyObject(vkDestroyBuffer, mVulkanSceneToBuild->mDeviceRef, mVulkanSceneToBuild->mSceneIndexBuffer);

	std::array bufferQueueFamilies = {mDeviceQueues->GetGraphicsQueueFamilyIndex()};

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

	ThrowIfFailed(vkCreateBuffer(mVulkanSceneToBuild->mDeviceRef, &indexBufferCreateInfo, nullptr, &mVulkanSceneToBuild->mSceneIndexBuffer));
}

void Vulkan::RenderableSceneBuilder::PreCreateConstantBuffer(size_t constantDataSize)
{
	SafeDestroyObject(vkDestroyBuffer, mVulkanSceneToBuild->mDeviceRef, mVulkanSceneToBuild->mSceneUniformBuffer);

	std::array bufferQueueFamilies = {mDeviceQueues->GetGraphicsQueueFamilyIndex()};

	//TODO: buffer device address
	//TODO: dedicated allocation
	VkBufferCreateInfo uniformBufferCreateInfo;
	uniformBufferCreateInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	uniformBufferCreateInfo.pNext                 = nullptr;
	uniformBufferCreateInfo.flags                 = 0;
	uniformBufferCreateInfo.size                  = constantDataSize;
	uniformBufferCreateInfo.usage                 = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	uniformBufferCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
	uniformBufferCreateInfo.queueFamilyIndexCount = (uint32_t)bufferQueueFamilies.size();
	uniformBufferCreateInfo.pQueueFamilyIndices   = bufferQueueFamilies.data();

	ThrowIfFailed(vkCreateBuffer(mVulkanSceneToBuild->mDeviceRef, &uniformBufferCreateInfo, nullptr, &mVulkanSceneToBuild->mSceneUniformBuffer));
}

void Vulkan::RenderableSceneBuilder::AllocateTextureMetadataArrays(size_t textureCount)
{
	mVulkanSceneToBuild->mSceneTextures.resize(textureCount);

	mSceneImageCreateInfos.resize(textureCount);
	mSceneTextureSubresourceSlices.resize(textureCount);

	mSceneImageCopyInfos.clear();
}

void Vulkan::RenderableSceneBuilder::LoadTextureFromFile(const std::wstring& textureFilename, uint64_t currentIntermediateBufferOffset, size_t textureIndex, std::vector<std::byte>& outTextureData)
{
	outTextureData.clear();

	std::unique_ptr<uint8_t[]>                             texData;
	std::vector<DDSTextureLoaderVk::LoadedSubresourceData> subresources;

	VkImage texture = VK_NULL_HANDLE;
	DDSTextureLoaderVk::LoadDDSTextureFromFile(mVulkanSceneToBuild->mDeviceRef, textureFilename.c_str(), &texture, texData, subresources, mDeviceParametersRef->GetDeviceProperties().limits.maxImageDimension2D, &mSceneImageCreateInfos[textureIndex]);

	mVulkanSceneToBuild->mSceneTextures[textureIndex] = texture;

	uint32_t subresourceSliceBegin = (uint32_t)(mSceneImageCopyInfos.size());
	uint32_t subresourceSliceEnd = (uint32_t)(mSceneImageCopyInfos.size() + subresources.size());
	mSceneTextureSubresourceSlices[textureIndex] = {.Begin = subresourceSliceBegin, .End = subresourceSliceEnd};
	mSceneImageCopyInfos.resize(mSceneImageCopyInfos.size() + subresources.size());

	VkDeviceSize currentOffset = (VkDeviceSize)currentIntermediateBufferOffset;
	for(size_t j = 0; j < subresources.size(); j++)
	{
		uint32_t globalSubresourceIndex = subresourceSliceBegin + j;

		currentOffset = outTextureData.size();
		outTextureData.insert(outTextureData.end(), subresources[j].PData, subresources[j].PData + subresources[j].DataByteSize);

		VkBufferImageCopy& bufferImageCopy = mSceneImageCopyInfos[globalSubresourceIndex];
		bufferImageCopy.bufferOffset                    = currentOffset + mIntermediateBufferTextureDataOffset;
		bufferImageCopy.bufferRowLength                 = 0;
		bufferImageCopy.bufferImageHeight               = 0;
		bufferImageCopy.imageSubresource.aspectMask     = subresources[j].SubresourceSlice.aspectMask;
		bufferImageCopy.imageSubresource.mipLevel       = subresources[j].SubresourceSlice.mipLevel;
		bufferImageCopy.imageSubresource.baseArrayLayer = subresources[j].SubresourceSlice.arrayLayer;
		bufferImageCopy.imageSubresource.layerCount     = 1;
		bufferImageCopy.imageOffset.x                   = 0;
		bufferImageCopy.imageOffset.y                   = 0;
		bufferImageCopy.imageOffset.z                   = 0;
		bufferImageCopy.imageExtent                     = subresources[j].Extent;
	}
}

void Vulkan::RenderableSceneBuilder::FinishBufferCreation()
{
	AllocateBuffersMemory();
}

void Vulkan::RenderableSceneBuilder::FinishTextureCreation()
{
	AllocateImageMemory();
	CreateImageViews();
}

std::byte* Vulkan::RenderableSceneBuilder::MapConstantBuffer()
{
	void* bufferPointer = nullptr;
	ThrowIfFailed(vkMapMemory(mVulkanSceneToBuild->mDeviceRef, mVulkanSceneToBuild->mBufferHostVisibleMemory, 0, VK_WHOLE_SIZE, 0, &bufferPointer));

	return reinterpret_cast<std::byte*>(bufferPointer);
}

void Vulkan::RenderableSceneBuilder::CreateIntermediateBuffer(uint64_t intermediateBufferSize) 
{
	std::array intermediateBufferQueues = {mDeviceQueues->GetGraphicsQueueFamilyIndex()};

	VkBufferCreateInfo intermediateBufferCreateInfo;
	intermediateBufferCreateInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	intermediateBufferCreateInfo.pNext                 = nullptr;
	intermediateBufferCreateInfo.flags                 = 0;
	intermediateBufferCreateInfo.size                  = intermediateBufferSize;
	intermediateBufferCreateInfo.usage                 = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	intermediateBufferCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
	intermediateBufferCreateInfo.queueFamilyIndexCount = (uint32_t)intermediateBufferQueues.size();
	intermediateBufferCreateInfo.pQueueFamilyIndices   = intermediateBufferQueues.data();

	vkCreateBuffer(mVulkanSceneToBuild->mDeviceRef, &intermediateBufferCreateInfo, nullptr, &mIntermediateBuffer);

	mIntermediateBufferMemory = mMemoryAllocator->AllocateIntermediateBufferMemory(mVulkanSceneToBuild->mDeviceRef, mIntermediateBuffer);

	//TODO: mGPU?
	VkBindBufferMemoryInfo bindBufferMemoryInfo;
	bindBufferMemoryInfo.sType        = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO;
	bindBufferMemoryInfo.pNext        = nullptr;
	bindBufferMemoryInfo.buffer       = mIntermediateBuffer;
	bindBufferMemoryInfo.memory       = mIntermediateBufferMemory;
	bindBufferMemoryInfo.memoryOffset = 0;
	
	std::array bindInfos = {bindBufferMemoryInfo};
	ThrowIfFailed(vkBindBufferMemory2(mVulkanSceneToBuild->mDeviceRef, (uint32_t)(bindInfos.size()), bindInfos.data()));
}

std::byte* Vulkan::RenderableSceneBuilder::MapIntermediateBuffer() const 
{
	void* bufferData = nullptr;
	ThrowIfFailed(vkMapMemory(mVulkanSceneToBuild->mDeviceRef, mIntermediateBufferMemory, 0, VK_WHOLE_SIZE, 0, &bufferData));

	return reinterpret_cast<std::byte*>(bufferData);
}

void Vulkan::RenderableSceneBuilder::UnmapIntermediateBuffer() const
{
	VkMappedMemoryRange mappedRange;
	mappedRange.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	mappedRange.pNext  = nullptr;
	mappedRange.memory = mIntermediateBufferMemory;
	mappedRange.offset = 0;
	mappedRange.size   = VK_WHOLE_SIZE;

	std::array mappedRanges = {mappedRange};
	ThrowIfFailed(vkFlushMappedMemoryRanges(mVulkanSceneToBuild->mDeviceRef, (uint32_t)(mappedRanges.size()), mappedRanges.data()));

	vkUnmapMemory(mVulkanSceneToBuild->mDeviceRef, mIntermediateBufferMemory);
}

void Vulkan::RenderableSceneBuilder::AllocateImageMemory()
{
	SafeDestroyObject(vkFreeMemory, mVulkanSceneToBuild->mDeviceRef, mVulkanSceneToBuild->mTextureMemory);

	std::vector<VkDeviceSize> textureMemoryOffsets;
	mVulkanSceneToBuild->mTextureMemory = mMemoryAllocator->AllocateImagesMemory(mVulkanSceneToBuild->mDeviceRef, mVulkanSceneToBuild->mSceneTextures, textureMemoryOffsets);

	std::vector<VkBindImageMemoryInfo> bindImageMemoryInfos(mVulkanSceneToBuild->mSceneTextures.size());
	for(size_t i = 0; i < mVulkanSceneToBuild->mSceneTextures.size(); i++)
	{
		//TODO: mGPU?
		bindImageMemoryInfos[i].sType        = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
		bindImageMemoryInfos[i].pNext        = nullptr;
		bindImageMemoryInfos[i].memory       = mVulkanSceneToBuild->mTextureMemory;
		bindImageMemoryInfos[i].memoryOffset = textureMemoryOffsets[i];
		bindImageMemoryInfos[i].image        = mVulkanSceneToBuild->mSceneTextures[i];
	}

	ThrowIfFailed(vkBindImageMemory2(mVulkanSceneToBuild->mDeviceRef, (uint32_t)(bindImageMemoryInfos.size()), bindImageMemoryInfos.data()));
}

void Vulkan::RenderableSceneBuilder::AllocateBuffersMemory()
{
	SafeDestroyObject(vkFreeMemory, mVulkanSceneToBuild->mDeviceRef, mVulkanSceneToBuild->mBufferMemory);
	SafeDestroyObject(vkFreeMemory, mVulkanSceneToBuild->mDeviceRef, mVulkanSceneToBuild->mBufferHostVisibleMemory);


	std::vector deviceLocalBuffers = { mVulkanSceneToBuild->mSceneVertexBuffer, mVulkanSceneToBuild->mSceneIndexBuffer};

	std::vector<VkDeviceSize> deviceLocalBufferOffsets;
	mVulkanSceneToBuild->mBufferMemory = mMemoryAllocator->AllocateBuffersMemory(mVulkanSceneToBuild->mDeviceRef, deviceLocalBuffers, MemoryManager::BufferAllocationType::DEVICE_LOCAL, deviceLocalBufferOffsets);

	std::vector<VkBindBufferMemoryInfo> bindBufferMemoryInfos(deviceLocalBuffers.size());
	for(size_t i = 0; i < deviceLocalBuffers.size(); i++)
	{
		//TODO: mGPU?
		bindBufferMemoryInfos[i].sType        = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO;
		bindBufferMemoryInfos[i].pNext        = nullptr;
		bindBufferMemoryInfos[i].buffer       = deviceLocalBuffers[i];
		bindBufferMemoryInfos[i].memory       = mVulkanSceneToBuild->mBufferMemory;
		bindBufferMemoryInfos[i].memoryOffset = deviceLocalBufferOffsets[i];
	}

	ThrowIfFailed(vkBindBufferMemory2(mVulkanSceneToBuild->mDeviceRef, (uint32_t)(bindBufferMemoryInfos.size()), bindBufferMemoryInfos.data()));


	std::vector hostVisibleBuffers = {mVulkanSceneToBuild->mSceneUniformBuffer};

	std::vector<VkDeviceSize> hostVisibleBufferOffsets;
	mVulkanSceneToBuild->mBufferHostVisibleMemory = mMemoryAllocator->AllocateBuffersMemory(mVulkanSceneToBuild->mDeviceRef, hostVisibleBuffers, MemoryManager::BufferAllocationType::HOST_VISIBLE, hostVisibleBufferOffsets);

	std::vector<VkBindBufferMemoryInfo> bindHostVisibleBufferMemoryInfos(hostVisibleBuffers.size());
	for(size_t i = 0; i < hostVisibleBuffers.size(); i++)
	{
		//TODO: mGPU?
		bindHostVisibleBufferMemoryInfos[i].sType        = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO;
		bindHostVisibleBufferMemoryInfos[i].pNext        = nullptr;
		bindHostVisibleBufferMemoryInfos[i].buffer       = hostVisibleBuffers[i];
		bindHostVisibleBufferMemoryInfos[i].memory       = mVulkanSceneToBuild->mBufferHostVisibleMemory;
		bindHostVisibleBufferMemoryInfos[i].memoryOffset = hostVisibleBufferOffsets[i];
	}

	ThrowIfFailed(vkBindBufferMemory2(mVulkanSceneToBuild->mDeviceRef, (uint32_t)(bindHostVisibleBufferMemoryInfos.size()), bindHostVisibleBufferMemoryInfos.data()));
}

void Vulkan::RenderableSceneBuilder::CreateImageViews()
{
	for(size_t i = 0; i < mVulkanSceneToBuild->mSceneTextures.size(); i++)
	{
		VkImageViewCreateInfo imageViewCreateInfo;
		imageViewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext                           = nullptr;
		imageViewCreateInfo.flags                           = 0;
		imageViewCreateInfo.image                           = mVulkanSceneToBuild->mSceneTextures[i];
		imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D; //Only 2D-textures are stored in mSceneTextures
		imageViewCreateInfo.format                          = mSceneImageCreateInfos[i].format;
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
		ThrowIfFailed(vkCreateImageView(mVulkanSceneToBuild->mDeviceRef, &imageViewCreateInfo, nullptr, &imageView));

		mVulkanSceneToBuild->mSceneTextureViews.push_back(imageView);
	}
}