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

Vulkan::RenderableSceneBuilder::RenderableSceneBuilder(RenderableScene* sceneToBuild): ModernRenderableSceneBuilder(sceneToBuild), mVulkanSceneToBuild(sceneToBuild)
{
	assert(sceneToBuild != nullptr);

	mIntermediateBuffer        = VK_NULL_HANDLE;
	mIntermediateBufferMemory  = VK_NULL_HANDLE;
}

Vulkan::RenderableSceneBuilder::~RenderableSceneBuilder()
{
	SafeDestroyObject(vkDestroyBuffer, mVulkanSceneToBuild->mDeviceRef, mIntermediateBuffer);
	SafeDestroyObject(vkFreeMemory,    mVulkanSceneToBuild->mDeviceRef, mIntermediateBufferMemory);
}

VkDeviceSize Vulkan::RenderableSceneBuilder::CreateSceneDataBuffers(const DeviceQueues* deviceQueues, VkDeviceSize currentIntermediateBuffeSize)
{
	SafeDestroyObject(vkDestroyBuffer, mVulkanSceneToBuild->mDeviceRef, mVulkanSceneToBuild->mSceneVertexBuffer);
	SafeDestroyObject(vkDestroyBuffer, mVulkanSceneToBuild->mDeviceRef, mVulkanSceneToBuild->mSceneIndexBuffer);
	SafeDestroyObject(vkDestroyBuffer, mVulkanSceneToBuild->mDeviceRef, mVulkanSceneToBuild->mSceneUniformBuffer);


	VkDeviceSize intermediateBufferSize = currentIntermediateBuffeSize;

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

	mIntermediateBufferVertexDataOffset = intermediateBufferSize;
	intermediateBufferSize             += vertexDataSize;


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

	mIntermediateBufferIndexDataOffset = intermediateBufferSize;
	intermediateBufferSize            += indexDataSize;


	VkDeviceSize uniformPerObjectDataSize = mSceneToBuild->mScenePerObjectData.size() * mSceneToBuild->mGBufferObjectChunkDataSize * Utils::InFlightFrameCount;
	VkDeviceSize uniformPerFrameDataSize  = mSceneToBuild->mGBufferFrameChunkDataSize * Utils::InFlightFrameCount;

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

	mSceneToBuild->mSceneDataConstantObjectBufferOffset = 0;
	mSceneToBuild->mSceneDataConstantFrameBufferOffset  = mSceneToBuild->mSceneDataConstantObjectBufferOffset + uniformPerObjectDataSize;


	return intermediateBufferSize;
}

VkDeviceSize Vulkan::RenderableSceneBuilder::LoadTextureImages(const std::vector<std::wstring>& sceneTextures, const DeviceParameters& deviceParameters, VkDeviceSize currentIntermediateBufferSize)
{
	VkDeviceSize intermediateBufferSize  = currentIntermediateBufferSize;
	mIntermediateBufferTextureDataOffset = intermediateBufferSize;

	DDSTextureLoaderVk::SetVkCreateImageFuncPtr(vkCreateImage);

	mTextureData.clear();

	VkDeviceSize currentOffset = currentIntermediateBufferSize;
	for(size_t i = 0; i < sceneTextures.size(); i++)
	{
		std::unique_ptr<uint8_t[]>                             texData;
		std::vector<DDSTextureLoaderVk::LoadedSubresourceData> texSubresources;

		VkImage texture = VK_NULL_HANDLE;
		VkImageCreateInfo textureCreateInfo;
		DDSTextureLoaderVk::LoadDDSTextureFromFile(mSceneToBuild->mDeviceRef, sceneTextures[i].c_str(), &texture, texData, texSubresources, deviceParameters.GetDeviceProperties().limits.maxImageDimension2D, &textureCreateInfo);

		mSceneToBuild->mSceneTextures.push_back(texture);
		mSceneTextureCreateInfos.push_back(std::move(textureCreateInfo));

		mSceneTextureCopyInfos.push_back(std::vector<VkBufferImageCopy>());
		for(size_t j = 0; j < texSubresources.size(); j++)
		{
			currentOffset = mTextureData.size();
			mTextureData.insert(mTextureData.end(), texSubresources[j].PData, texSubresources[j].PData + texSubresources[j].DataByteSize);

			VkBufferImageCopy copyRegion;
			copyRegion.bufferOffset                    = currentOffset + mIntermediateBufferTextureDataOffset;
			copyRegion.bufferRowLength                 = 0;
			copyRegion.bufferImageHeight               = 0;
			copyRegion.imageSubresource.aspectMask     = texSubresources[j].SubresourceSlice.aspectMask;
			copyRegion.imageSubresource.mipLevel       = texSubresources[j].SubresourceSlice.mipLevel;
			copyRegion.imageSubresource.baseArrayLayer = texSubresources[j].SubresourceSlice.arrayLayer;
			copyRegion.imageSubresource.layerCount     = 1;
			copyRegion.imageOffset.x                   = 0;
			copyRegion.imageOffset.y                   = 0;
			copyRegion.imageOffset.z                   = 0;
			copyRegion.imageExtent                     = texSubresources[j].Extent;

			mSceneTextureCopyInfos.back().push_back(copyRegion);
		}
	}

	return mIntermediateBufferTextureDataOffset + mTextureData.size() * sizeof(uint8_t);
}

void Vulkan::RenderableSceneBuilder::AllocateImageMemory(const MemoryManager* memoryAllocator)
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

void Vulkan::RenderableSceneBuilder::AllocateBuffersMemory(const MemoryManager* memoryAllocator)
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

	ThrowIfFailed(vkMapMemory(mSceneToBuild->mDeviceRef, mSceneToBuild->mBufferHostVisibleMemory, 0, VK_WHOLE_SIZE, 0, &mSceneToBuild->mSceneConstantDataBufferPointer));
}

void Vulkan::RenderableSceneBuilder::CreateImageViews()
{
	for(size_t i = 0; i < mSceneToBuild->mSceneTextures.size(); i++)
	{
		VkImageViewCreateInfo imageViewCreateInfo;
		imageViewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext                           = nullptr;
		imageViewCreateInfo.flags                           = 0;
		imageViewCreateInfo.image                           = mSceneToBuild->mSceneTextures[i];
		imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D; //Only 2D-textures are stored in mSceneTextures
		imageViewCreateInfo.format                          = mSceneTextureCreateInfos[i].format;
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

void Vulkan::RenderableSceneBuilder::CreateIntermediateBuffer(const DeviceQueues* deviceQueues, const MemoryManager* memoryAllocator, VkDeviceSize intermediateBufferSize)
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

void Vulkan::RenderableSceneBuilder::FillIntermediateBufferData()
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