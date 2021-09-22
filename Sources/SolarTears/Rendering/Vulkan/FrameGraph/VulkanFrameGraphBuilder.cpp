#include "VulkanFrameGraphBuilder.hpp"
#include "VulkanFrameGraph.hpp"
#include "../VulkanInstanceParameters.hpp"
#include "../VulkanWorkerCommandBuffers.hpp"
#include "../VulkanFunctions.hpp"
#include "../VulkanMemory.hpp"
#include "../VulkanSwapChain.hpp"
#include "../VulkanDeviceQueues.hpp"
#include "../VulkanShaders.hpp"
#include <VulkanGenericStructures.h>
#include <algorithm>
#include <numeric>

#include "Passes/VulkanGBufferPass.hpp"
#include "Passes/VulkanCopyImagePass.hpp"

Vulkan::FrameGraphBuilder::FrameGraphBuilder(LoggerQueue* logger, FrameGraph* graphToBuild, FrameGraphDescription&& frameGraphDescription, const SwapChain* swapchain): ModernFrameGraphBuilder(graphToBuild, std::move(frameGraphDescription)), mLogger(logger), mVulkanGraphToBuild(graphToBuild), mSwapChain(swapchain)
{
	mImageViewCount = 0;

	InitPassTable();

	mShaderDatabase = std::make_unique<ShaderDatabase>(mLogger);
}

Vulkan::FrameGraphBuilder::~FrameGraphBuilder()
{
}

void Vulkan::FrameGraphBuilder::SetPassSubresourceFormat(const std::string_view subresourceId, VkFormat format)
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	mSubresourceInfos[subresourceInfoIndex].Format = format;
}

void Vulkan::FrameGraphBuilder::SetPassSubresourceLayout(const std::string_view subresourceId, VkImageLayout layout)
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	mSubresourceInfos[subresourceInfoIndex].Layout = layout;
}

void Vulkan::FrameGraphBuilder::SetPassSubresourceUsage(const std::string_view subresourceId, VkImageUsageFlags usage)
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	mSubresourceInfos[subresourceInfoIndex].Usage = usage;
}

void Vulkan::FrameGraphBuilder::SetPassSubresourceAspectFlags(const std::string_view subresourceId, VkImageAspectFlags aspect)
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	mSubresourceInfos[subresourceInfoIndex].Aspect = aspect;
}

void Vulkan::FrameGraphBuilder::SetPassSubresourceStageFlags(const std::string_view subresourceId, VkPipelineStageFlags stage)
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	mSubresourceInfos[subresourceInfoIndex].Stage = stage;
}

void Vulkan::FrameGraphBuilder::SetPassSubresourceAccessFlags(const std::string_view subresourceId, VkAccessFlags accessFlags)
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	mSubresourceInfos[subresourceInfoIndex].Access = accessFlags;
}

void Vulkan::FrameGraphBuilder::EnableSubresourceAutoBeforeBarrier(const std::string_view subresourceId, bool autoBarrier)
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);

	SubresourceMetadataNode& metadataNode = mSubresourceMetadataNodesFlat[mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr)];
	if(autoBarrier)
	{
		metadataNode.Flags |= TextureFlagAutoBeforeBarrier;
	}
	else
	{
		metadataNode.Flags &= ~TextureFlagAutoBeforeBarrier;
	}
}

void Vulkan::FrameGraphBuilder::EnableSubresourceAutoAfterBarrier(const std::string_view subresourceId, bool autoBarrier)
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);

	SubresourceMetadataNode& metadataNode = mSubresourceMetadataNodesFlat[mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr)];
	if(autoBarrier)
	{
		metadataNode.Flags |= TextureFlagAutoAfterBarrier;
	}
	else
	{
		metadataNode.Flags &= ~TextureFlagAutoAfterBarrier;
	}
}

const Vulkan::DeviceQueues* Vulkan::FrameGraphBuilder::GetDeviceQueues() const
{
	return mDeviceQueues;
}

const Vulkan::SwapChain* Vulkan::FrameGraphBuilder::GetSwapChain() const
{
	return mSwapChain;
}

const Vulkan::DeviceParameters* Vulkan::FrameGraphBuilder::GetDeviceParameters() const
{
	return mDeviceParameters;
}

Vulkan::ShaderDatabase* Vulkan::FrameGraphBuilder::GetShaderDatabase() const
{
	return mShaderDatabase.get();
}

VkImage Vulkan::FrameGraphBuilder::GetRegisteredResource(const std::string_view passName, const std::string_view subresourceId, uint32_t frame) const
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);

	const SubresourceMetadataNode& metadataNode = mSubresourceMetadataNodesFlat[subresourceInfoIndex];
	if(metadataNode.FirstFrameHandle == (uint32_t)(-1))
	{
		return VK_NULL_HANDLE;
	}
	else
	{
		if(metadataNode.FirstFrameHandle == GetBackbufferImageSpan().Begin)
		{
			uint32_t passPeriod = mRenderPassOwnPeriods.at(std::string(passName));
			return mVulkanGraphToBuild->mImages[metadataNode.FirstFrameHandle + frame / passPeriod];
		}
		else
		{
			
			return mVulkanGraphToBuild->mImages[metadataNode.FirstFrameHandle + frame % metadataNode.FrameCount];
		}
	}
}

VkImageView Vulkan::FrameGraphBuilder::GetRegisteredSubresource(const std::string_view passName, const std::string_view subresourceId, uint32_t frame) const
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);

	const SubresourceMetadataNode& metadataNode = mSubresourceMetadataNodesFlat[subresourceInfoIndex];
	if(metadataNode.FirstFrameViewHandle == (uint32_t)(-1))
	{
		return VK_NULL_HANDLE;
	}
	else
	{
		if(metadataNode.FirstFrameHandle == GetBackbufferImageSpan().Begin)
		{
			uint32_t passPeriod = mRenderPassOwnPeriods.at(std::string(passName));
			return mVulkanGraphToBuild->mImageViews[metadataNode.FirstFrameHandle + frame / passPeriod];
		}
		else
		{
			
			return mVulkanGraphToBuild->mImageViews[metadataNode.FirstFrameHandle + frame % metadataNode.FrameCount];
		}
	}
}

VkFormat Vulkan::FrameGraphBuilder::GetRegisteredSubresourceFormat(const std::string_view subresourceId) const
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	return mSubresourceInfos[subresourceInfoIndex].Format;
}

VkImageLayout Vulkan::FrameGraphBuilder::GetRegisteredSubresourceLayout(const std::string_view subresourceId) const
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	return mSubresourceInfos[subresourceInfoIndex].Layout;
}

VkImageUsageFlags Vulkan::FrameGraphBuilder::GetRegisteredSubresourceUsage(const std::string_view subresourceId) const
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	return mSubresourceInfos[subresourceInfoIndex].Usage;
}

VkPipelineStageFlags Vulkan::FrameGraphBuilder::GetRegisteredSubresourceStageFlags(const std::string_view subresourceId) const
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	return mSubresourceInfos[subresourceInfoIndex].Stage;
}

VkImageAspectFlags Vulkan::FrameGraphBuilder::GetRegisteredSubresourceAspectFlags(const std::string_view subresourceId) const
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	return mSubresourceInfos[subresourceInfoIndex].Aspect;
}

VkAccessFlags Vulkan::FrameGraphBuilder::GetRegisteredSubresourceAccessFlags(const std::string_view subresourceId) const
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	return mSubresourceInfos[subresourceInfoIndex].Access;
}

VkImageLayout Vulkan::FrameGraphBuilder::GetPreviousPassSubresourceLayout(const std::string_view subresourceId) const
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	return mSubresourceInfos[subresourceInfoIndex].Layout;
}

VkImageUsageFlags Vulkan::FrameGraphBuilder::GetPreviousPassSubresourceUsage(const std::string_view subresourceId) const
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	return mSubresourceInfos[subresourceInfoIndex].Usage;
}

VkPipelineStageFlags Vulkan::FrameGraphBuilder::GetPreviousPassSubresourceStageFlags(const std::string_view subresourceId) const
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	return mSubresourceInfos[subresourceInfoIndex].Stage;
}

VkImageAspectFlags Vulkan::FrameGraphBuilder::GetPreviousPassSubresourceAspectFlags(const std::string_view subresourceId) const
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	return mSubresourceInfos[subresourceInfoIndex].Aspect;
}

VkAccessFlags Vulkan::FrameGraphBuilder::GetPreviousPassSubresourceAccessFlags(const std::string_view subresourceId) const
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	return mSubresourceInfos[subresourceInfoIndex].Access;
}

VkImageLayout Vulkan::FrameGraphBuilder::GetNextPassSubresourceLayout(const std::string_view subresourceId) const
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	return mSubresourceInfos[subresourceInfoIndex].Layout;
}

VkImageUsageFlags Vulkan::FrameGraphBuilder::GetNextPassSubresourceUsage(const std::string_view subresourceId) const
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	return mSubresourceInfos[subresourceInfoIndex].Usage;
}

VkPipelineStageFlags Vulkan::FrameGraphBuilder::GetNextPassSubresourceStageFlags(const std::string_view subresourceId) const
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	return mSubresourceInfos[subresourceInfoIndex].Stage;
}

VkImageAspectFlags Vulkan::FrameGraphBuilder::GetNextPassSubresourceAspectFlags(const std::string_view subresourceId) const
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	return mSubresourceInfos[subresourceInfoIndex].Aspect;
}

VkAccessFlags Vulkan::FrameGraphBuilder::GetNextPassSubresourceAccessFlags(const std::string_view subresourceId) const
{
	FrameGraphDescription::SubresourceId subresourceIdStr(subresourceId);
	uint32_t subresourceInfoIndex = mMetadataNodeIndicesPerSubresourceIds.at(subresourceIdStr);
	return mSubresourceInfos[subresourceInfoIndex].Access;
}

void Vulkan::FrameGraphBuilder::Build(const FrameGraphBuildInfo& buildInfo)
{
	mInstanceParameters   = buildInfo.InstanceParams;
	mDeviceParameters     = buildInfo.DeviceParams;
	mMemoryManager        = buildInfo.MemoryAllocator;
	mDeviceQueues         = buildInfo.Queues;
	mWorkerCommandBuffers = buildInfo.CommandBuffers;

	ModernFrameGraphBuilder::Build();
}

void Vulkan::FrameGraphBuilder::BuildPassDescriptorSets()
{
	std::vector<PassSetInfo> uniquePassSetInfos;
	std::vector<uint16_t>    passSetBindingTypes;
	mShaderDatabase->BuildUniquePassSetInfos(uniquePassSetInfos, passSetBindingTypes);

	for(size_t i = 0; i < uniquePassSetInfos.size(); i++)
	{

	}

	for(const FrameGraphDescription::RenderPassName& passName: mSortedRenderPassNames)
	{
		RenderPassType passType = mFrameGraphDescription.GetPassType(passName);

	}

	//At this point we have the RenderPassType -> {layout, bindings} infos
	//We only need to know how many

	//for(PassSetInfo& passSetInfo: uniquePassSetInfos)
	//{
	//	
	//}



	//mVulkanGraphToBuild->mDescriptorSets.resize(descriptorSetCount, VK_NULL_HANDLE);
}

void Vulkan::FrameGraphBuilder::CreateTextures(const std::vector<TextureResourceCreateInfo>& textureCreateInfos, const std::vector<TextureResourceCreateInfo>& backbufferCreateInfos, uint32_t totalTextureCount) const
{
	mVulkanGraphToBuild->mImages.resize(totalTextureCount);
	std::vector<VkImageMemoryBarrier> imageInitialStateBarriers(totalTextureCount); //Barriers to initialize images

	std::vector<VkImage> memoryAllocateImages;
	for(const TextureResourceCreateInfo& textureCreateInfo: textureCreateInfos)
	{
		uint32_t headNodeIndex    = textureCreateInfo.MetadataHeadIndex;
		uint32_t currentNodeIndex = headNodeIndex;

		VkFormat headFormat = mSubresourceInfos[headNodeIndex].Format;

		VkImageUsageFlags  usageFlags       = 0;
		VkImageCreateFlags imageCreateFlags = 0;
		std::array         queueIndices     = {mDeviceQueues->GetGraphicsQueueFamilyIndex()};
		do
		{
			const SubresourceMetadataNode& subresourceMetadata = mSubresourceMetadataNodesFlat[currentNodeIndex];
			const SubresourceInfo&         subresourceInfo     = mSubresourceInfos[currentNodeIndex];

			queueIndices[0] = PassClassToQueueIndex(subresourceMetadata.PassClass);
			usageFlags     |= subresourceInfo.Usage;
			
			//TODO: mutable format lists
			if(subresourceInfo.Format != headFormat)
			{
				imageCreateFlags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
			}

		} while(currentNodeIndex != headNodeIndex);


		const SubresourceMetadataNode& headNode = mSubresourceMetadataNodesFlat[headNodeIndex];

		uint32_t        lastNodeIndex   = headNode.PrevPassNodeIndex;
		RenderPassClass lastPassClass   = mSubresourceMetadataNodesFlat[lastNodeIndex].PassClass;
		VkImageLayout   lastLayout      = mSubresourceInfos[lastNodeIndex].Layout;
		VkAccessFlags   lastAccessFlags = mSubresourceInfos[lastNodeIndex].Access;
		VkAccessFlags   lastAspectMask  = mSubresourceInfos[lastNodeIndex].Aspect;


		VkImageCreateInfo imageCreateInfo;
		imageCreateInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.pNext                 = nullptr;
		imageCreateInfo.flags                 = imageCreateFlags;
		imageCreateInfo.imageType             = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format                = headFormat;
		imageCreateInfo.extent.width          = GetConfig()->GetViewportWidth();
		imageCreateInfo.extent.height         = GetConfig()->GetViewportHeight();
		imageCreateInfo.extent.depth          = 1;
		imageCreateInfo.mipLevels             = 1;
		imageCreateInfo.arrayLayers           = 1;
		imageCreateInfo.samples               = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling                = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage                 = usageFlags;
		imageCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.queueFamilyIndexCount = (uint32_t)(queueIndices.size());
		imageCreateInfo.pQueueFamilyIndices   = queueIndices.data();
		imageCreateInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;

		for(uint32_t frame = 0; frame < headNode.FrameCount; frame++)
		{
			VkImage image = VK_NULL_HANDLE;
			ThrowIfFailed(vkCreateImage(mVulkanGraphToBuild->mDeviceRef, &imageCreateInfo, nullptr, &image));

#if defined(DEBUG) || defined(_DEBUG)
			if(mInstanceParameters->IsDebugUtilsExtensionEnabled())
			{
				if(headNode.FrameCount == 1)
				{
					VulkanUtils::SetDebugObjectName(mVulkanGraphToBuild->mDeviceRef, image, textureCreateInfo.Name);
				}
				else
				{
					VulkanUtils::SetDebugObjectName(mVulkanGraphToBuild->mDeviceRef, image, FrameGraphDescription::RenderPassName(textureCreateInfo.Name) + std::to_string(frame));
				}
			}
#endif

			VkImageMemoryBarrier imageBarrier;
			imageBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageBarrier.pNext                           = nullptr;
			imageBarrier.srcAccessMask                   = 0;
			imageBarrier.dstAccessMask                   = lastAccessFlags;
			imageBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
			imageBarrier.newLayout                       = lastLayout;
			imageBarrier.srcQueueFamilyIndex             = queueIndices[0]; //Resources were created with that queue ownership
			imageBarrier.dstQueueFamilyIndex             = PassClassToQueueIndex(lastPassClass);
			imageBarrier.image                           = image;
			imageBarrier.subresourceRange.aspectMask     = lastAspectMask;
			imageBarrier.subresourceRange.baseMipLevel   = 0;
			imageBarrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
			imageBarrier.subresourceRange.baseArrayLayer = 0;
			imageBarrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

			mVulkanGraphToBuild->mImages[headNode.FirstFrameHandle + frame] = image;
			imageInitialStateBarriers[headNode.FirstFrameHandle + frame]    = imageBarrier;

			memoryAllocateImages.push_back(image);
		}
	}

	for(const TextureResourceCreateInfo& backbufferCreateInfo: backbufferCreateInfos)
	{
		const SubresourceMetadataNode& headNode = mSubresourceMetadataNodesFlat[backbufferCreateInfo.MetadataHeadIndex];

		for(uint32_t frame = 0; frame < headNode.FrameCount; frame++)
		{
			VkImage swapchainImage = mSwapChain->GetSwapchainImage(frame);

#if defined(DEBUG) || defined(_DEBUG)
			VulkanUtils::SetDebugObjectName(mVulkanGraphToBuild->mDeviceRef, swapchainImage, FrameGraphDescription::RenderPassName(backbufferCreateInfo.Name) + std::to_string(frame));
#endif

			VkImageMemoryBarrier imageBarrier;
			imageBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			imageBarrier.pNext                           = nullptr;
			imageBarrier.srcAccessMask                   = 0;
			imageBarrier.dstAccessMask                   = 0;
			imageBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
			imageBarrier.newLayout                       = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			imageBarrier.srcQueueFamilyIndex             = 0; 
			imageBarrier.dstQueueFamilyIndex             = PassClassToQueueIndex(RenderPassClass::Present);
			imageBarrier.image                           = swapchainImage;
			imageBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
			imageBarrier.subresourceRange.baseMipLevel   = 0;
			imageBarrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
			imageBarrier.subresourceRange.baseArrayLayer = 0;
			imageBarrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

			mVulkanGraphToBuild->mImages[headNode.FirstFrameHandle + frame] = swapchainImage;
			imageInitialStateBarriers[headNode.FirstFrameHandle + frame]    = imageBarrier;
		}
	}


	SafeDestroyObject(vkFreeMemory, mVulkanGraphToBuild->mDeviceRef, mVulkanGraphToBuild->mImageMemory);

	std::vector<VkDeviceSize> textureMemoryOffsets;
	mVulkanGraphToBuild->mImageMemory = mMemoryManager->AllocateImagesMemory(mVulkanGraphToBuild->mDeviceRef, memoryAllocateImages, textureMemoryOffsets);

	std::vector<VkBindImageMemoryInfo> bindImageMemoryInfos(memoryAllocateImages.size());
	for(size_t i = 0; i < memoryAllocateImages.size(); i++)
	{
		//TODO: mGPU?
		bindImageMemoryInfos[i].sType        = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
		bindImageMemoryInfos[i].pNext        = nullptr;
		bindImageMemoryInfos[i].memory       = mVulkanGraphToBuild->mImageMemory;
		bindImageMemoryInfos[i].memoryOffset = textureMemoryOffsets[i];
		bindImageMemoryInfos[i].image        = memoryAllocateImages[i];
	}

	ThrowIfFailed(vkBindImageMemory2(mVulkanGraphToBuild->mDeviceRef, (uint32_t)(bindImageMemoryInfos.size()), bindImageMemoryInfos.data()));


	VkCommandPool   graphicsCommandPool   = mWorkerCommandBuffers->GetMainThreadGraphicsCommandPool(0);
	VkCommandBuffer graphicsCommandBuffer = mWorkerCommandBuffers->GetMainThreadGraphicsCommandBuffer(0);

	ThrowIfFailed(vkResetCommandPool(mVulkanGraphToBuild->mDeviceRef, graphicsCommandPool, 0));

	//TODO: mGPU?
	VkCommandBufferBeginInfo graphicsCmdBufferBeginInfo;
	graphicsCmdBufferBeginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	graphicsCmdBufferBeginInfo.pNext            = nullptr;
	graphicsCmdBufferBeginInfo.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	graphicsCmdBufferBeginInfo.pInheritanceInfo = nullptr;

	ThrowIfFailed(vkBeginCommandBuffer(graphicsCommandBuffer, &graphicsCmdBufferBeginInfo));

	std::array<VkMemoryBarrier,       0> memoryBarriers;
	std::array<VkBufferMemoryBarrier, 0> bufferBarriers;
	vkCmdPipelineBarrier(graphicsCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
		                 (uint32_t)memoryBarriers.size(),            memoryBarriers.data(),
		                 (uint32_t)bufferBarriers.size(),            bufferBarriers.data(),
		                 (uint32_t)imageInitialStateBarriers.size(), imageInitialStateBarriers.data());

	ThrowIfFailed(vkEndCommandBuffer(graphicsCommandBuffer));

	mDeviceQueues->GraphicsQueueSubmit(graphicsCommandBuffer);

	mDeviceQueues->GraphicsQueueWait(); //TODO: may wait after build finished
}

uint32_t Vulkan::FrameGraphBuilder::AddSubresourceMetadata()
{
	SubresourceInfo subresourceInfo;
	subresourceInfo.Format = VK_FORMAT_UNDEFINED;
	subresourceInfo.Layout = VK_IMAGE_LAYOUT_UNDEFINED;
	subresourceInfo.Usage  = 0;
	subresourceInfo.Aspect = 0;
	subresourceInfo.Stage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	subresourceInfo.Access = 0;

	mSubresourceInfos.push_back(subresourceInfo);
	return (uint32_t)(mSubresourceInfos.size() - 1);
}

uint32_t Vulkan::FrameGraphBuilder::AddPresentSubresourceMetadata()
{
	SubresourceInfo subresourceInfo;
	subresourceInfo.Format = mSwapChain->GetBackbufferFormat();
	subresourceInfo.Layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	subresourceInfo.Usage  = 0;
	subresourceInfo.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceInfo.Stage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	subresourceInfo.Access = 0;

	mSubresourceInfos.push_back(subresourceInfo);
	return (uint32_t)(mSubresourceInfos.size() - 1);
}

bool Vulkan::FrameGraphBuilder::IsReadSubresource(uint32_t subresourceInfoIndex)
{
	VkAccessFlags readAccessFlags = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT 
		                          |	VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT 
		                          |	VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_HOST_READ_BIT | VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT 
		                          | VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT | VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR 
		                          | VK_ACCESS_SHADING_RATE_IMAGE_READ_BIT_NV | VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT | VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV;

	return mSubresourceInfos[subresourceInfoIndex].Access & readAccessFlags;
}

bool Vulkan::FrameGraphBuilder::IsWriteSubresource(uint32_t subresourceInfoIndex)
{
	VkAccessFlags writeAccessFlags = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT 
		                           | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT 
		                           | VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

	return mSubresourceInfos[subresourceInfoIndex].Access & writeAccessFlags;
}

void Vulkan::FrameGraphBuilder::RegisterPassInGraph(RenderPassType passType, const FrameGraphDescription::RenderPassName& passName)
{
	auto passResourceRegisterFunc = mPassAddFuncTable.at(passType);
	passResourceRegisterFunc(this, passName);

	auto passShaderRegisterFunc = mPassRegisterShadersFuncTable.at(passType);
	passShaderRegisterFunc(mShaderDatabase.get());
}

void Vulkan::FrameGraphBuilder::CreatePassObject(const FrameGraphDescription::RenderPassName& passName, RenderPassType passType, uint32_t frame)
{
	auto passCreateFunc = mPassCreateFuncTable.at(passType);
	mVulkanGraphToBuild->mRenderPasses.emplace_back(passCreateFunc(mVulkanGraphToBuild->mDeviceRef, this, passName, frame));
}

uint32_t Vulkan::FrameGraphBuilder::NextPassSpanId()
{
	return (uint32_t)mVulkanGraphToBuild->mRenderPasses.size();
}

bool Vulkan::FrameGraphBuilder::ValidateSubresourceViewParameters(uint32_t currNodeIndex, uint32_t prevNodeIndex)
{
	const SubresourceMetadataNode& prevPassNode = mSubresourceMetadataNodesFlat[prevNodeIndex];
	SubresourceMetadataNode& currPassNode = mSubresourceMetadataNodesFlat[currNodeIndex];

	SubresourceInfo& prevPassSubresourceInfo = mSubresourceInfos[prevNodeIndex];
	SubresourceInfo& thisPassSubresourceInfo = mSubresourceInfos[currNodeIndex];
	
	bool dataPropagated = false;
	if(thisPassSubresourceInfo.Format == VK_FORMAT_UNDEFINED && prevPassSubresourceInfo.Format != VK_FORMAT_UNDEFINED)
	{
		thisPassSubresourceInfo.Format = prevPassSubresourceInfo.Format;

		dataPropagated = true;
	}

	if(thisPassSubresourceInfo.Aspect == 0 && prevPassSubresourceInfo.Aspect != 0)
	{
		thisPassSubresourceInfo.Aspect = prevPassSubresourceInfo.Aspect;

		dataPropagated = true;
	}

	if(PassClassToQueueIndex(currPassNode.PassClass) == PassClassToQueueIndex(prevPassNode.PassClass) && thisPassSubresourceInfo.Layout == prevPassSubresourceInfo.Layout && thisPassSubresourceInfo.Access != prevPassSubresourceInfo.Access)
	{
		thisPassSubresourceInfo.Access |= prevPassSubresourceInfo.Access;
		prevPassSubresourceInfo.Access |= thisPassSubresourceInfo.Access;

		dataPropagated = true;
	}

	if(thisPassSubresourceInfo.Usage != prevPassSubresourceInfo.Usage)
	{
		thisPassSubresourceInfo.Usage |= prevPassSubresourceInfo.Usage;
		prevPassSubresourceInfo.Usage |= thisPassSubresourceInfo.Usage;

		dataPropagated = true;
	}

	currPassNode.ViewSortKey = ((uint64_t)thisPassSubresourceInfo.Aspect << 32) | (uint32_t)thisPassSubresourceInfo.Format;
	return dataPropagated;
}

void Vulkan::FrameGraphBuilder::AllocateImageViews(const std::vector<uint64_t>& sortKeys, uint32_t frameCount, std::vector<uint32_t>& outViewIds)
{
	outViewIds.reserve(sortKeys.size());
	for(uint64_t sortKey: sortKeys)
	{
		uint32_t imageViewId = (uint32_t)(-1);
		if(sortKey != 0)
		{
			imageViewId = mImageViewCount;
			mImageViewCount += frameCount;
		}

		outViewIds.push_back(imageViewId);
	}
}

void Vulkan::FrameGraphBuilder::CreateTextureViews(const std::vector<TextureSubresourceCreateInfo>& textureViewCreateInfos) const
{
	mVulkanGraphToBuild->mImageViews.resize(mImageViewCount);
	for(const TextureSubresourceCreateInfo& textureViewCreateInfo: textureViewCreateInfos)
	{
		VkImage image = mVulkanGraphToBuild->mImages[textureViewCreateInfo.ImageIndex];
		uint32_t subresourceInfoIndex = textureViewCreateInfo.SubresourceInfoIndex;

		mVulkanGraphToBuild->mImageViews[textureViewCreateInfo.ImageViewIndex] = CreateImageView(image, subresourceInfoIndex);
	}
}

uint32_t Vulkan::FrameGraphBuilder::AddBeforePassBarrier(uint32_t metadataIndex)
{
	/*
	*    1. Same queue family, layout unchanged:          No barrier needed
	*    2. Same queue family, layout changed to PRESENT: No barrier needed, handled by the previous state's barrier
	*    3. Same queue family, layout changed:            Need a barrier Old layout -> New layout
	*
	*    4. Queue family changed, layout unchanged: Need an acquire barrier with new access mask
	*    5. Queue family changed, layout changed:   Need an acquire + layout change barrier
	*    6. Queue family changed, from present:     Need an acquire barrier with new access mask
	*    7. Queue family changed, to present:       Need an acquire barrier with source and destination access masks 0
	*/

	const SubresourceMetadataNode& currMetadataNode = mSubresourceMetadataNodesFlat[metadataIndex];
	const SubresourceMetadataNode& prevMetadataNode = mSubresourceMetadataNodesFlat[currMetadataNode.PrevPassNodeIndex];

	const SubresourceInfo& prevPassInfo = mSubresourceInfos[currMetadataNode.PrevPassNodeIndex];
	const SubresourceInfo& currPassInfo = mSubresourceInfos[metadataIndex];

	bool barrierNeeded = false;
	VkAccessFlags prevAccessMask = prevPassInfo.Access;
	VkAccessFlags currAccessMask = currPassInfo.Access;

	if(PassClassToQueueIndex(prevMetadataNode.PassClass) == PassClassToQueueIndex(currMetadataNode.PassClass)) //Rules 1, 2, 3
	{
		barrierNeeded = (currPassInfo.Layout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) && (prevPassInfo.Layout != currPassInfo.Layout);
	}
	else //Rules 4, 5, 6, 7
	{
		prevAccessMask = 0; //Access mask should be set to 0 in an acquire barrier change

		barrierNeeded = true;
	}

	if(barrierNeeded)
	{
		VkImageMemoryBarrier imageMemoryBarrier;
		imageMemoryBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.pNext                           = nullptr;
		imageMemoryBarrier.srcAccessMask                   = prevAccessMask;
		imageMemoryBarrier.dstAccessMask                   = currAccessMask;
		imageMemoryBarrier.oldLayout                       = prevPassInfo.Layout;
		imageMemoryBarrier.newLayout                       = currPassInfo.Layout;
		imageMemoryBarrier.srcQueueFamilyIndex             = PassClassToQueueIndex(prevMetadataNode.PassClass);
		imageMemoryBarrier.dstQueueFamilyIndex             = PassClassToQueueIndex(currMetadataNode.PassClass);
		imageMemoryBarrier.image                           = mVulkanGraphToBuild->mImages[currMetadataNode.FirstFrameHandle];
		imageMemoryBarrier.subresourceRange.aspectMask     = prevPassInfo.Aspect | currPassInfo.Aspect;
		imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
		imageMemoryBarrier.subresourceRange.layerCount     = 1;
		imageMemoryBarrier.subresourceRange.baseMipLevel   = 0;
		imageMemoryBarrier.subresourceRange.levelCount     = 1;

		mVulkanGraphToBuild->mImageBarriers.push_back(imageMemoryBarrier);

		return (uint32_t)(mVulkanGraphToBuild->mImageBarriers.size() - 1);
	}

	return (uint32_t)(-1);
}

uint32_t Vulkan::FrameGraphBuilder::AddAfterPassBarrier(uint32_t metadataIndex)
{
	/*
	*    1. Same queue family, layout unchanged:           No barrier needed
	*    2. Same queue family, layout changed to PRESENT:  Need a barrier Old layout -> New Layout
	*    3. Same queue family, layout changed other cases: No barrier needed, will be handled by the next state's barrier
	*
	*    4. Queue family changed, layout unchanged: Need a release barrier with old access mask
	*    5. Queue family changed, layout changed:   Need a release + layout change barrier
	*    6. Queue family changed, to present:       Need a release barrier with old access mask
	*    7. Queue family changed, from present:     Need a release barrier with source and destination access masks 0
	*/

	const SubresourceMetadataNode& currMetadataNode = mSubresourceMetadataNodesFlat[metadataIndex];
	const SubresourceMetadataNode& nextMetadataNode = mSubresourceMetadataNodesFlat[currMetadataNode.NextPassNodeIndex];

	const SubresourceInfo& currPassInfo = mSubresourceInfos[metadataIndex];
	const SubresourceInfo& nextPassInfo = mSubresourceInfos[currMetadataNode.NextPassNodeIndex];

	bool barrierNeeded = false;
	VkAccessFlags currAccessMask = currPassInfo.Access;
	VkAccessFlags nextAccessMask = nextPassInfo.Access;
	if(PassClassToQueueIndex(currMetadataNode.PassClass) == PassClassToQueueIndex(nextMetadataNode.PassClass)) //Rules 1, 2, 3
	{
		barrierNeeded = (nextPassInfo.Layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	}
	else //Rules 4, 5, 6, 7
	{
		nextAccessMask = 0; //Access mask should be set to 0 in a release barrier change

		barrierNeeded = true;
	}
 
	if(barrierNeeded)
	{
		VkImageMemoryBarrier imageMemoryBarrier;
		imageMemoryBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.pNext                           = nullptr;
		imageMemoryBarrier.srcAccessMask                   = currAccessMask;
		imageMemoryBarrier.dstAccessMask                   = nextAccessMask;
		imageMemoryBarrier.oldLayout                       = currPassInfo.Layout;
		imageMemoryBarrier.newLayout                       = nextPassInfo.Layout;
		imageMemoryBarrier.srcQueueFamilyIndex             = PassClassToQueueIndex(currMetadataNode.PassClass);
		imageMemoryBarrier.dstQueueFamilyIndex             = PassClassToQueueIndex(nextMetadataNode.PassClass);
		imageMemoryBarrier.image                           = mVulkanGraphToBuild->mImages[currMetadataNode.FirstFrameHandle];
		imageMemoryBarrier.subresourceRange.aspectMask     = currPassInfo.Aspect | nextPassInfo.Aspect;
		imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
		imageMemoryBarrier.subresourceRange.layerCount     = 1;
		imageMemoryBarrier.subresourceRange.baseMipLevel   = 0;
		imageMemoryBarrier.subresourceRange.levelCount     = 1;

		mVulkanGraphToBuild->mImageBarriers.push_back(imageMemoryBarrier);

		return (uint32_t)(mVulkanGraphToBuild->mImageBarriers.size() - 1);
	}

	return (uint32_t)(-1);
}

void Vulkan::FrameGraphBuilder::InitializeTraverseData() const
{
	mVulkanGraphToBuild->mFrameRecordedGraphicsCommandBuffers.resize(mVulkanGraphToBuild->mGraphicsPassSpans.size());
}

uint32_t Vulkan::FrameGraphBuilder::GetSwapchainImageCount() const
{
	return mSwapChain->SwapchainImageCount;
}

void Vulkan::FrameGraphBuilder::InitPassTable()
{
	mPassAddFuncTable.clear();
	mPassCreateFuncTable.clear();

	AddPassToTable<GBufferPass>();
	AddPassToTable<CopyImagePass>();
}

uint32_t Vulkan::FrameGraphBuilder::PassClassToQueueIndex(RenderPassClass passClass) const
{
	switch(passClass)
	{
	case RenderPassClass::Graphics:
		return mDeviceQueues->GetGraphicsQueueFamilyIndex();

	case RenderPassClass::Compute:
		return mDeviceQueues->GetComputeQueueFamilyIndex();

	case RenderPassClass::Transfer:
		return mDeviceQueues->GetTransferQueueFamilyIndex();

	case RenderPassClass::Present:
		return mSwapChain->GetPresentQueueFamilyIndex();

	default:
		return mDeviceQueues->GetGraphicsQueueFamilyIndex();
	}
}

VkImageView Vulkan::FrameGraphBuilder::CreateImageView(VkImage image, uint32_t subresourceInfoIndex) const
{
	//TODO: fragment density map
	VkImageViewCreateInfo imageViewCreateInfo;
	imageViewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.pNext                           = nullptr;
	imageViewCreateInfo.flags                           = 0;
	imageViewCreateInfo.image                           = image;
	imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format                          = mSubresourceInfos[subresourceInfoIndex].Format;
	imageViewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.subresourceRange.aspectMask     = mSubresourceInfos[subresourceInfoIndex].Aspect;

	imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
	imageViewCreateInfo.subresourceRange.levelCount     = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount     = 1;

	VkImageView imageView = VK_NULL_HANDLE;
	ThrowIfFailed(vkCreateImageView(mVulkanGraphToBuild->mDeviceRef, &imageViewCreateInfo, nullptr, &imageView));

	return imageView;
}
