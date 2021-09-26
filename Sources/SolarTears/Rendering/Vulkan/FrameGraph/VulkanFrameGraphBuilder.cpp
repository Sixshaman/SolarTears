#include "VulkanFrameGraphBuilder.hpp"
#include "VulkanFrameGraph.hpp"
#include "../VulkanInstanceParameters.hpp"
#include "../VulkanWorkerCommandBuffers.hpp"
#include "../VulkanFunctions.hpp"
#include "../VulkanMemory.hpp"
#include "../VulkanSwapChain.hpp"
#include "../VulkanDeviceQueues.hpp"
#include "../VulkanShaders.hpp"
#include "VulkanRenderPassDispatchFuncs.hpp"
#include <VulkanGenericStructures.h>
#include <algorithm>
#include <numeric>

Vulkan::FrameGraphBuilder::FrameGraphBuilder(LoggerQueue* logger, FrameGraph* graphToBuild, FrameGraphDescription&& frameGraphDescription, const SwapChain* swapchain): ModernFrameGraphBuilder(graphToBuild, std::move(frameGraphDescription)), mLogger(logger), mVulkanGraphToBuild(graphToBuild), mSwapChain(swapchain)
{
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

const VkDevice Vulkan::FrameGraphBuilder::GetDevice() const
{
	return mVulkanGraphToBuild->mDeviceRef;
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


	//Go through all passes.
	//For each pass, get its type.
	//For the pass type, get the subresources requiring creating a descriptor from uniquePassSetInfos
	//Allocate sets, etc.
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

void Vulkan::FrameGraphBuilder::CreateTextures()
{
	mVulkanGraphToBuild->mImages.clear();

	std::vector<VkImageMemoryBarrier> imageInitialStateBarriers; //Barriers to initialize images
	for(auto& textureMetadataPair: mResourceMetadatas)
	{
		const FrameGraphDescription::ResourceName& textureName = textureMetadataPair.first;
		ResourceMetadata& textureMetadata = textureMetadataPair.second;

		if(textureName == mBackbufferName)
		{
			continue; //Backbuffer is handled separately
		}

		std::array<uint32_t, 1> imageQueueIndices = {mDeviceQueues->GetGraphicsQueueFamilyIndex()};

		VkImageCreateInfo imageCreateInfo;
		imageCreateInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.pNext                 = nullptr;
		imageCreateInfo.flags                 = 0;
		imageCreateInfo.imageType             = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format                = VK_FORMAT_UNDEFINED;
		imageCreateInfo.extent.width          = GetConfig()->GetViewportWidth();
		imageCreateInfo.extent.height         = GetConfig()->GetViewportHeight();
		imageCreateInfo.extent.depth          = 1;
		imageCreateInfo.mipLevels             = 1;
		imageCreateInfo.arrayLayers           = 1;
		imageCreateInfo.samples               = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling                = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage                 = 0;
		imageCreateInfo.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.queueFamilyIndexCount = (uint32_t)imageQueueIndices.size();
		imageCreateInfo.pQueueFamilyIndices   = imageQueueIndices.data();
		imageCreateInfo.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;

		VkImageMemoryBarrier imageInitialBarrier;
		imageInitialBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageInitialBarrier.pNext                           = nullptr;
		imageInitialBarrier.srcAccessMask                   = 0;
		imageInitialBarrier.dstAccessMask                   = 0;
		imageInitialBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInitialBarrier.newLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInitialBarrier.srcQueueFamilyIndex             = imageQueueIndices[0]; //Resources were created with that queue ownership
		imageInitialBarrier.dstQueueFamilyIndex             = imageQueueIndices[0];
		imageInitialBarrier.image                           = VK_NULL_HANDLE;
		imageInitialBarrier.subresourceRange.aspectMask     = 0;
		imageInitialBarrier.subresourceRange.baseMipLevel   = 0;
		imageInitialBarrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
		imageInitialBarrier.subresourceRange.baseArrayLayer = 0;
		imageInitialBarrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;


		uint8_t formatChanges = 0;

		uint32_t headNodeIndex    = textureMetadata.HeadNodeIndex;
		uint32_t currentNodeIndex = headNodeIndex;
		do
		{
			const SubresourceMetadataNode& subresourceMetadata = mSubresourceMetadataNodesFlat[currentNodeIndex];
			const SubresourceInfo&         subresourceInfo     = mSubresourceInfosFlat[subresourceMetadata.SubresourceInfoIndex];

			imageQueueIndices[0] = PassClassToQueueIndex(subresourceMetadata.PassClass);
			imageCreateInfo.usage |= subresourceInfo.Usage;
			
			imageInitialBarrier.dstAccessMask               = subresourceInfo.Access;
			imageInitialBarrier.dstQueueFamilyIndex         = PassClassToQueueIndex(subresourceMetadata.PassClass);
			imageInitialBarrier.subresourceRange.aspectMask = subresourceInfo.Aspect;

			//TODO: mutable format lists
			if(subresourceInfo.Format != VK_FORMAT_UNDEFINED)
			{
				if(imageCreateInfo.format != subresourceInfo.Format)
				{
					assert(formatChanges < 255);
					formatChanges++;
				}

				imageCreateInfo.format = subresourceInfo.Format;
			}

			if(subresourceInfo.Layout != VK_FORMAT_UNDEFINED)
			{
				imageInitialBarrier.newLayout = subresourceInfo.Layout;
			}

			currentNodeIndex = subresourceMetadata.NextPassNodeIndex;

		} while(currentNodeIndex != headNodeIndex);

		if(formatChanges > 1)
		{
			imageCreateInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
		}

		uint32_t newImageIndex = (uint32_t)mVulkanGraphToBuild->mImages.size();
		for(uint32_t frame = 0; frame < textureMetadata.FrameCount; frame++)
		{
			VkImage image = VK_NULL_HANDLE;
			ThrowIfFailed(vkCreateImage(mVulkanGraphToBuild->mDeviceRef, &imageCreateInfo, nullptr, &image));

			imageInitialBarrier.image = image;
			imageInitialStateBarriers.push_back(imageInitialBarrier);

#if defined(DEBUG) || defined(_DEBUG)
			if(mInstanceParameters->IsDebugUtilsExtensionEnabled())
			{
				if(textureMetadata.FrameCount == 1)
				{
					VulkanUtils::SetDebugObjectName(mVulkanGraphToBuild->mDeviceRef, image, textureMetadata.Name);
				}
				else
				{
					VulkanUtils::SetDebugObjectName(mVulkanGraphToBuild->mDeviceRef, image, FrameGraphDescription::RenderPassName(textureMetadata.Name) + std::to_string(frame));
				}
			}
#endif
		}

		textureMetadata.FirstFrameHandle = newImageIndex;
	}


	//Allocate the memory
	SafeDestroyObject(vkFreeMemory, mVulkanGraphToBuild->mDeviceRef, mVulkanGraphToBuild->mImageMemory);

	std::vector<VkDeviceSize> textureMemoryOffsets;
	mVulkanGraphToBuild->mImageMemory = mMemoryManager->AllocateImagesMemory(mVulkanGraphToBuild->mDeviceRef, mVulkanGraphToBuild->mImages, textureMemoryOffsets);

	std::vector<VkBindImageMemoryInfo> bindImageMemoryInfos(mVulkanGraphToBuild->mImages.size());
	for(size_t i = 0; i < mVulkanGraphToBuild->mImages.size(); i++)
	{
		//TODO: mGPU?
		bindImageMemoryInfos[i].sType        = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
		bindImageMemoryInfos[i].pNext        = nullptr;
		bindImageMemoryInfos[i].memory       = mVulkanGraphToBuild->mImageMemory;
		bindImageMemoryInfos[i].memoryOffset = textureMemoryOffsets[i];
		bindImageMemoryInfos[i].image        = mVulkanGraphToBuild->mImages[i];
	}

	ThrowIfFailed(vkBindImageMemory2(mVulkanGraphToBuild->mDeviceRef, (uint32_t)(bindImageMemoryInfos.size()), bindImageMemoryInfos.data()));


	//Process the backbuffer
	ResourceMetadata& backbufferMetadata = mResourceMetadatas.at(mBackbufferName);

	uint32_t backbufferImageIndex = (uint32_t)mVulkanGraphToBuild->mImages.size();
	backbufferMetadata.FirstFrameHandle = backbufferImageIndex;
	for(uint32_t frame = 0; frame < backbufferMetadata.FrameCount; frame++)
	{
		VkImage swapchainImage = mSwapChain->GetSwapchainImage(frame);

#if defined(DEBUG) || defined(_DEBUG)
		VulkanUtils::SetDebugObjectName(mVulkanGraphToBuild->mDeviceRef, swapchainImage, FrameGraphDescription::RenderPassName(backbufferMetadata.Name) + std::to_string(frame));
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

		mVulkanGraphToBuild->mImages.push_back(swapchainImage);
		imageInitialStateBarriers.push_back(imageBarrier);
	}


	//Barrier the images
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

void Vulkan::FrameGraphBuilder::RegisterPassTypes(const std::span<RenderPassType>& passTypes)
{
	for(RenderPassType passType: passTypes)
	{
		uint32_t passSubresourceCount = GetPassSubresourceTypeCount(passType);
		Span<uint32_t> passSubresourceInfoSpan =
		{
			.Begin = (uint32_t)mSubresourceInfosFlat.size(),
			.End   = (uint32_t)(mSubresourceInfosFlat.size() + passSubresourceCount)
		};

		mRenderPassSubresourceInfoSpans[passType] = passSubresourceInfoSpan;
		mSubresourceInfosFlat.resize(mSubresourceInfosFlat.size() + passSubresourceCount);

		for(size_t subresourceIndex = passSubresourceInfoSpan.Begin; subresourceIndex < passSubresourceInfoSpan.End; subresourceIndex++)
		{
			uint32_t passSubresourceIndex = subresourceIndex - passSubresourceInfoSpan.Begin;

			mSubresourceInfosFlat[subresourceIndex].Format = GetPassSubresourceFormat(passType, passSubresourceIndex);
			mSubresourceInfosFlat[subresourceIndex].Aspect = GetPassSubresourceAspect(passType, passSubresourceIndex);
			mSubresourceInfosFlat[subresourceIndex].Layout = GetPassSubresourceLayout(passType, passSubresourceIndex);
			mSubresourceInfosFlat[subresourceIndex].Usage  = GetPassSubresourceUsage(passType,  passSubresourceIndex);
			mSubresourceInfosFlat[subresourceIndex].Stage  = GetPassSubresourceStage(passType,  passSubresourceIndex);
			mSubresourceInfosFlat[subresourceIndex].Access = GetPassSubresourceAccess(passType, passSubresourceIndex);
		}
	}
}

bool Vulkan::FrameGraphBuilder::IsReadSubresource(uint32_t subresourceInfoIndex)
{
	VkAccessFlags readAccessFlags = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT 
		                          |	VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT 
		                          |	VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_HOST_READ_BIT | VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ_BIT_EXT 
		                          | VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT | VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR 
		                          | VK_ACCESS_SHADING_RATE_IMAGE_READ_BIT_NV | VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT | VK_ACCESS_COMMAND_PREPROCESS_READ_BIT_NV;

	return mSubresourceInfosFlat[subresourceInfoIndex].Access & readAccessFlags;
}

bool Vulkan::FrameGraphBuilder::IsWriteSubresource(uint32_t subresourceInfoIndex)
{
	VkAccessFlags writeAccessFlags = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT 
		                           | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_TRANSFORM_FEEDBACK_WRITE_BIT_EXT 
		                           | VK_ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE_BIT_EXT | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

	return mSubresourceInfosFlat[subresourceInfoIndex].Access & writeAccessFlags;
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

void Vulkan::FrameGraphBuilder::CreateTextureViews()
{
	mVulkanGraphToBuild->mImageViews.clear();
	for(const auto& textureMetadataPair: mResourceMetadatas)
	{
		std::unordered_map<uint64_t, uint32_t> imageViewIndicesForViewInfos; //To only create different image views if the format + aspect flags differ

		const ResourceMetadata& resourceMetadata = textureMetadataPair.second;

		uint32_t headNodeIndex = resourceMetadata.HeadNodeIndex;
		uint32_t currNodeIndex = headNodeIndex;
		do
		{
			SubresourceMetadataNode& subresourceMetadata = mSubresourceMetadataNodesFlat[currNodeIndex];
			const SubresourceInfo& subresourceInfo = mSubresourceInfosFlat[subresourceMetadata.SubresourceInfoIndex];

			if(subresourceInfo.Format == VK_FORMAT_UNDEFINED || subresourceInfo.Aspect == 0)
			{
				//The resource doesn't require a view in the pass
				continue;
			}

			uint64_t viewInfoKey = (subresourceInfo.Aspect << 32) | (subresourceInfo.Format);
			
			auto imageViewIt = imageViewIndicesForViewInfos.find(viewInfoKey);
			if(imageViewIt != imageViewIndicesForViewInfos.end())
			{
				subresourceMetadata.FirstFrameViewHandle = imageViewIt->second;
			}
			else
			{
				uint32_t newImageViewIndex = (uint32_t)mVulkanGraphToBuild->mImageViews.size();
				mVulkanGraphToBuild->mImageViews.resize(mVulkanGraphToBuild->mImageViews.size() + resourceMetadata.FrameCount);

				for(uint32_t frameIndex = 0; frameIndex < resourceMetadata.FrameCount; frameIndex++)
				{
					VkImage image = mVulkanGraphToBuild->mImages[resourceMetadata.FirstFrameHandle + frameIndex];
					mVulkanGraphToBuild->mImageViews[newImageViewIndex + frameIndex] = CreateImageView(image, subresourceInfo.Format, subresourceInfo.Aspect);
				}

				subresourceMetadata.FirstFrameViewHandle  = newImageViewIndex;
				imageViewIndicesForViewInfos[viewInfoKey] = newImageViewIndex;
			}
			
			currNodeIndex = subresourceMetadata.NextPassNodeIndex;

		} while(currNodeIndex != headNodeIndex);
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


	if (PassClassToQueueIndex(currPassNode.PassClass) == PassClassToQueueIndex(prevPassNode.PassClass) && thisPassSubresourceInfo.Layout == prevPassSubresourceInfo.Layout && thisPassSubresourceInfo.Access != prevPassSubresourceInfo.Access)
	{
		thisPassSubresourceInfo.Access |= prevPassSubresourceInfo.Access;
		prevPassSubresourceInfo.Access |= thisPassSubresourceInfo.Access;

		dataPropagated = true;
	}

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

	
			if (PassClassToQueueIndex(currPassNode.PassClass) == PassClassToQueueIndex(prevPassNode.PassClass) && thisPassSubresourceInfo.Layout == prevPassSubresourceInfo.Layout && thisPassSubresourceInfo.Access != prevPassSubresourceInfo.Access)
			{
				thisPassSubresourceInfo.Access |= prevPassSubresourceInfo.Access;
				prevPassSubresourceInfo.Access |= thisPassSubresourceInfo.Access;

				dataPropagated = true;
			}

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

VkImageView Vulkan::FrameGraphBuilder::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect) const
{
	//TODO: fragment density map
	VkImageViewCreateInfo imageViewCreateInfo;
	imageViewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.pNext                           = nullptr;
	imageViewCreateInfo.flags                           = 0;
	imageViewCreateInfo.image                           = image;
	imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format                          = format;
	imageViewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.subresourceRange.aspectMask     = aspect;

	imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
	imageViewCreateInfo.subresourceRange.levelCount     = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount     = 1;

	VkImageView imageView = VK_NULL_HANDLE;
	ThrowIfFailed(vkCreateImageView(mVulkanGraphToBuild->mDeviceRef, &imageViewCreateInfo, nullptr, &imageView));

	return imageView;
}
