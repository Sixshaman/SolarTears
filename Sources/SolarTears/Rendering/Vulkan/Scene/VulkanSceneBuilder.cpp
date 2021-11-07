#include "VulkanSceneBuilder.hpp"
#include "../VulkanUtils.hpp"
#include "../VulkanFunctions.hpp"
#include "../VulkanMemory.hpp"
#include "../VulkanShaders.hpp"
#include "../VulkanDeviceQueues.hpp"
#include "../VulkanWorkerCommandBuffers.hpp"
#include "../../Common/RenderingUtils.hpp"
#include "../../../../3rdParty/DDSTextureLoaderVk/DDSTextureLoaderVk.h"
#include <array>
#include <cassert>

Vulkan::RenderableSceneBuilder::RenderableSceneBuilder(RenderableScene* sceneToBuild, MemoryManager* memoryAllocator, DeviceQueues* deviceQueues, 
	                                                   WorkerCommandBuffers* workerCommandBuffers, const DeviceParameters* deviceParameters): ModernRenderableSceneBuilder(sceneToBuild, 1), mVulkanSceneToBuild(sceneToBuild), mMemoryAllocator(memoryAllocator),
                                                                                                                                              mDeviceQueues(deviceQueues), mWorkerCommandBuffers(workerCommandBuffers), mDeviceParametersRef(deviceParameters)
{
	assert(sceneToBuild != nullptr);

	mIntermediateBuffer        = VK_NULL_HANDLE;
	mIntermediateBufferMemory  = VK_NULL_HANDLE;

	DDSTextureLoaderVk::SetVkCreateImageFuncPtr(vkCreateImage);
}

Vulkan::RenderableSceneBuilder::~RenderableSceneBuilder()
{
	SafeDestroyObject(vkDestroyBuffer, mVulkanSceneToBuild->mDeviceRef, mIntermediateBuffer);
	SafeDestroyObject(vkFreeMemory,    mVulkanSceneToBuild->mDeviceRef, mIntermediateBufferMemory);
}

void Vulkan::RenderableSceneBuilder::CreateVertexBufferInfo(size_t vertexDataSize)
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

void Vulkan::RenderableSceneBuilder::CreateIndexBufferInfo(size_t indexDataSize)
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

void Vulkan::RenderableSceneBuilder::CreateConstantBufferInfo(size_t constantDataSize)
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
	uniformBufferCreateInfo.usage                 = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	uniformBufferCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
	uniformBufferCreateInfo.queueFamilyIndexCount = (uint32_t)bufferQueueFamilies.size();
	uniformBufferCreateInfo.pQueueFamilyIndices   = bufferQueueFamilies.data();

	ThrowIfFailed(vkCreateBuffer(mVulkanSceneToBuild->mDeviceRef, &uniformBufferCreateInfo, nullptr, &mVulkanSceneToBuild->mSceneUniformBuffer));
}

void Vulkan::RenderableSceneBuilder::CreateUploadBufferInfo(size_t uploadDataSize)
{
	SafeDestroyObject(vkDestroyBuffer, mVulkanSceneToBuild->mDeviceRef, mVulkanSceneToBuild->mSceneUploadBuffer);

	std::array bufferQueueFamilies = {mDeviceQueues->GetGraphicsQueueFamilyIndex()};

	//TODO: buffer device address
	//TODO: dedicated allocation
	VkBufferCreateInfo uniformBufferCreateInfo;
	uniformBufferCreateInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	uniformBufferCreateInfo.pNext                 = nullptr;
	uniformBufferCreateInfo.flags                 = 0;
	uniformBufferCreateInfo.size                  = uploadDataSize;
	uniformBufferCreateInfo.usage                 = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	uniformBufferCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
	uniformBufferCreateInfo.queueFamilyIndexCount = (uint32_t)bufferQueueFamilies.size();
	uniformBufferCreateInfo.pQueueFamilyIndices   = bufferQueueFamilies.data();

	ThrowIfFailed(vkCreateBuffer(mVulkanSceneToBuild->mDeviceRef, &uniformBufferCreateInfo, nullptr, &mVulkanSceneToBuild->mSceneUploadBuffer));
}

void Vulkan::RenderableSceneBuilder::AllocateTextureMetadataArrays(size_t textureCount)
{
	mVulkanSceneToBuild->mSceneTextures.resize(textureCount);

	mSceneImageFormats.resize(textureCount);
	mSceneTextureSubresourceSpans.resize(textureCount);

	mSceneImageCopyInfos.clear();
}

void Vulkan::RenderableSceneBuilder::LoadTextureFromFile(const std::wstring& textureFilename, uint64_t currentIntermediateBufferOffset, size_t textureIndex, std::vector<std::byte>& outTextureData)
{
	outTextureData.clear();

	std::unique_ptr<uint8_t[]>                             texData;
	std::vector<DDSTextureLoaderVk::LoadedSubresourceData> subresources;

	VkImage texture = VK_NULL_HANDLE;
	VkImageCreateInfo createInfo;
	DDSTextureLoaderVk::LoadDDSTextureFromFile(mVulkanSceneToBuild->mDeviceRef, textureFilename.c_str(), &texture, texData, subresources, mDeviceParametersRef->GetDeviceProperties().limits.maxImageDimension2D, &createInfo);

	mVulkanSceneToBuild->mSceneTextures[textureIndex] = texture;
	mSceneImageFormats[textureIndex] = createInfo.format;

	uint32_t subresourceSliceBegin = (uint32_t)(mSceneImageCopyInfos.size());
	uint32_t subresourceSliceEnd = (uint32_t)(mSceneImageCopyInfos.size() + subresources.size());
	mSceneTextureSubresourceSpans[textureIndex] = {.Begin = subresourceSliceBegin, .End = subresourceSliceEnd};
	mSceneImageCopyInfos.resize(mSceneImageCopyInfos.size() + subresources.size());

	VkDeviceSize currentOffset = (VkDeviceSize)currentIntermediateBufferOffset;
	for(size_t j = 0; j < subresources.size(); j++)
	{
		uint32_t globalSubresourceIndex = subresourceSliceBegin + (uint32_t)j;

		currentOffset = outTextureData.size();
		outTextureData.insert(outTextureData.end(), reinterpret_cast<const std::byte*>(subresources[j].PData), reinterpret_cast<const std::byte*>(subresources[j].PData + subresources[j].DataByteSize));

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

	mVulkanSceneToBuild->mCurrFrameUploadCopyRegions.resize(mRigidObjectCount + 1); //One for frame data update and one for each potential object data update
}

void Vulkan::RenderableSceneBuilder::FinishTextureCreation()
{
	AllocateImageMemory();
	CreateImageViews();
}

std::byte* Vulkan::RenderableSceneBuilder::MapUploadBuffer()
{
	void* bufferPointer = nullptr;
	ThrowIfFailed(vkMapMemory(mVulkanSceneToBuild->mDeviceRef, mVulkanSceneToBuild->mBufferHostVisibleMemory, 0, VK_WHOLE_SIZE, 0, &bufferPointer));

	return reinterpret_cast<std::byte*>(bufferPointer);
}

void Vulkan::RenderableSceneBuilder::CreateIntermediateBuffer() 
{
	std::array intermediateBufferQueues = {mDeviceQueues->GetGraphicsQueueFamilyIndex()};

	VkBufferCreateInfo intermediateBufferCreateInfo;
	intermediateBufferCreateInfo.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	intermediateBufferCreateInfo.pNext                 = nullptr;
	intermediateBufferCreateInfo.flags                 = 0;
	intermediateBufferCreateInfo.size                  = mIntermediateBufferSize;
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

void Vulkan::RenderableSceneBuilder::WriteInitializationCommands() const
{
	VkCommandPool   graphicsCommandPool   = mWorkerCommandBuffers->GetMainThreadGraphicsCommandPool(0);
	VkCommandBuffer graphicsCommandBuffer = mWorkerCommandBuffers->GetMainThreadGraphicsCommandBuffer(0);

	VkPipelineStageFlags invalidateStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

	//Baking
	ThrowIfFailed(vkResetCommandPool(mVulkanSceneToBuild->mDeviceRef, graphicsCommandPool, 0));

	//TODO: mGPU?
	VkCommandBufferBeginInfo graphicsCmdBufferBeginInfo;
	graphicsCmdBufferBeginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	graphicsCmdBufferBeginInfo.pNext            = nullptr;
	graphicsCmdBufferBeginInfo.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	graphicsCmdBufferBeginInfo.pInheritanceInfo = nullptr;

	ThrowIfFailed(vkBeginCommandBuffer(graphicsCommandBuffer, &graphicsCmdBufferBeginInfo));

	std::array sceneBuffers           = {mVulkanSceneToBuild->mSceneVertexBuffer, mVulkanSceneToBuild->mSceneIndexBuffer, mVulkanSceneToBuild->mSceneUniformBuffer};
	std::array sceneBufferAccessMasks = {VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,     VK_ACCESS_INDEX_READ_BIT,               VK_ACCESS_UNIFORM_READ_BIT};

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
		imageTransferBarriers[i].srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
		imageTransferBarriers[i].dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
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
		bufferTransferBarriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferTransferBarriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferTransferBarriers[i].buffer              = sceneBuffers[i];
		bufferTransferBarriers[i].offset              = 0;
		bufferTransferBarriers[i].size                = VK_WHOLE_SIZE;
	}

	std::array<VkMemoryBarrier, 0> memoryTransferBarriers;
	vkCmdPipelineBarrier(graphicsCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
		                 (uint32_t)memoryTransferBarriers.size(), memoryTransferBarriers.data(),
		                 (uint32_t)bufferTransferBarriers.size(), bufferTransferBarriers.data(),
		                 (uint32_t)imageTransferBarriers.size(),  imageTransferBarriers.data());


	for(size_t i = 0; i < mVulkanSceneToBuild->mSceneTextures.size(); i++)
	{
		Span<uint32_t> subresourceSpan = mSceneTextureSubresourceSpans[i];
		vkCmdCopyBufferToImage(graphicsCommandBuffer, mIntermediateBuffer, mVulkanSceneToBuild->mSceneTextures[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceSpan.End - subresourceSpan.Begin, mSceneImageCopyInfos.data() + subresourceSpan.Begin);
	}

	std::array sceneBufferDataOffsets = {mIntermediateBufferVertexDataOffset,                      mIntermediateBufferIndexDataOffset,                     mIntermediateBufferStaticConstantDataOffset};
	std::array sceneBufferDataSizes   = {mVertexBufferData.size() * sizeof(RenderableSceneVertex), mIndexBufferData.size() * sizeof(RenderableSceneIndex), mStaticConstantData.size() * sizeof(std::byte)};
	for(size_t i = 0; i < sceneBuffers.size(); i++)
	{
		VkBufferCopy copyRegion;
		copyRegion.srcOffset = sceneBufferDataOffsets[i];
		copyRegion.dstOffset = 0;
		copyRegion.size      = sceneBufferDataSizes[i];

		std::array copyRegions = {copyRegion};
		vkCmdCopyBuffer(graphicsCommandBuffer, mIntermediateBuffer, sceneBuffers[i], (uint32_t)(copyRegions.size()), copyRegions.data());
	}

	std::vector<VkImageMemoryBarrier> imageValidateBarriers(mVulkanSceneToBuild->mSceneTextures.size());
	for(size_t i = 0; i < mVulkanSceneToBuild->mSceneTextures.size(); i++)
	{
		imageValidateBarriers[i].sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageValidateBarriers[i].pNext                           = nullptr;
		imageValidateBarriers[i].srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageValidateBarriers[i].dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;
		imageValidateBarriers[i].oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageValidateBarriers[i].newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageValidateBarriers[i].srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
		imageValidateBarriers[i].dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
		imageValidateBarriers[i].image                           = mVulkanSceneToBuild->mSceneTextures[i];
		imageValidateBarriers[i].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		imageValidateBarriers[i].subresourceRange.baseMipLevel   = 0;
		imageValidateBarriers[i].subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
		imageValidateBarriers[i].subresourceRange.baseArrayLayer = 0;
		imageValidateBarriers[i].subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;
	}

	std::array<VkBufferMemoryBarrier, sceneBuffers.size()> bufferValidateBarriers;
	for(size_t i = 0; i < sceneBuffers.size(); i++)
	{
		bufferValidateBarriers[i].sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		bufferValidateBarriers[i].pNext               = nullptr;
		bufferValidateBarriers[i].srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
		bufferValidateBarriers[i].dstAccessMask       = sceneBufferAccessMasks[i];
		bufferValidateBarriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferValidateBarriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferValidateBarriers[i].buffer              = sceneBuffers[i];
		bufferValidateBarriers[i].offset              = 0;
		bufferValidateBarriers[i].size                = VK_WHOLE_SIZE;
	}

	std::array<VkMemoryBarrier, 0> memoryValidateBarriers;
	vkCmdPipelineBarrier(graphicsCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, invalidateStageMask, 0,
		                 (uint32_t)memoryValidateBarriers.size(), memoryValidateBarriers.data(),
		                 (uint32_t)bufferValidateBarriers.size(), bufferValidateBarriers.data(),
		                 (uint32_t)imageValidateBarriers.size(), imageValidateBarriers.data());

	ThrowIfFailed(vkEndCommandBuffer(graphicsCommandBuffer));
}

void Vulkan::RenderableSceneBuilder::SubmitInitializationCommands() const
{
	VkCommandBuffer graphicsCommandBuffer = mWorkerCommandBuffers->GetMainThreadGraphicsCommandBuffer(0);
	mDeviceQueues->GraphicsQueueSubmit(graphicsCommandBuffer);
}

void Vulkan::RenderableSceneBuilder::WaitForInitializationCommands() const
{
	mDeviceQueues->GraphicsQueueWait();
}

void Vulkan::RenderableSceneBuilder::AllocateImageMemory()
{
	SafeDestroyObject(vkFreeMemory, mVulkanSceneToBuild->mDeviceRef, mVulkanSceneToBuild->mTextureMemory);

	std::vector<VkBindImageMemoryInfo> bindImageMemoryInfos(mVulkanSceneToBuild->mSceneTextures.size());
	for(size_t i = 0; i < mVulkanSceneToBuild->mSceneTextures.size(); i++)
	{
		//TODO: mGPU?
		bindImageMemoryInfos[i].sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
		bindImageMemoryInfos[i].pNext = nullptr;
		bindImageMemoryInfos[i].image = mVulkanSceneToBuild->mSceneTextures[i];
	}

	mVulkanSceneToBuild->mTextureMemory = mMemoryAllocator->AllocateImagesMemory(mVulkanSceneToBuild->mDeviceRef, bindImageMemoryInfos);
}

void Vulkan::RenderableSceneBuilder::AllocateBuffersMemory()
{
	SafeDestroyObject(vkFreeMemory, mVulkanSceneToBuild->mDeviceRef, mVulkanSceneToBuild->mBufferMemory);
	SafeDestroyObject(vkFreeMemory, mVulkanSceneToBuild->mDeviceRef, mVulkanSceneToBuild->mBufferHostVisibleMemory);


	std::vector deviceLocalBuffers = {mVulkanSceneToBuild->mSceneVertexBuffer, mVulkanSceneToBuild->mSceneIndexBuffer, mVulkanSceneToBuild->mSceneUniformBuffer};

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


	std::vector hostVisibleBuffers = {mVulkanSceneToBuild->mSceneUploadBuffer};

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
		imageViewCreateInfo.format                          = mSceneImageFormats[i];
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