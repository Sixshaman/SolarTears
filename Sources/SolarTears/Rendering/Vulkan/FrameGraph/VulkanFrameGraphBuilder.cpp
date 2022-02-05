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

Vulkan::FrameGraphBuilder::FrameGraphBuilder(LoggerQueue* logger, FrameGraph* graphToBuild, SamplerManager* samplerManager, const SwapChain* swapchain): ModernFrameGraphBuilder(graphToBuild), mLogger(logger), mVulkanGraphToBuild(graphToBuild), mSwapChain(swapchain)
{
	mShaderDatabase = std::make_unique<ShaderDatabase>(mVulkanGraphToBuild->mDeviceRef, samplerManager, mLogger);
}

Vulkan::FrameGraphBuilder::~FrameGraphBuilder()
{
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

VkImage Vulkan::FrameGraphBuilder::GetRegisteredResource(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mTotalPassMetadatas[passIndex].SubresourceMetadataSpan;

	const SubresourceMetadataNode& metadataNode = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex];
	return mVulkanGraphToBuild->mImages[metadataNode.ResourceMetadataIndex];
}

VkImageView Vulkan::FrameGraphBuilder::GetRegisteredSubresource(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mTotalPassMetadatas[passIndex].SubresourceMetadataSpan;

	const SubresourceMetadataPayload& metadataPayload = mSubresourceMetadataPayloads[metadataSpan.Begin + subresourceIndex];
	if(metadataPayload.ImageViewIndex == (uint32_t)(-1))
	{
		return VK_NULL_HANDLE;
	}
	else
	{
		return mVulkanGraphToBuild->mImageViews[metadataPayload.ImageViewIndex];
	}
}

VkFormat Vulkan::FrameGraphBuilder::GetRegisteredSubresourceFormat(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mTotalPassMetadatas[passIndex].SubresourceMetadataSpan;
	return mSubresourceMetadataPayloads[metadataSpan.Begin + subresourceIndex].Format;
}

VkImageLayout Vulkan::FrameGraphBuilder::GetRegisteredSubresourceLayout(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mTotalPassMetadatas[passIndex].SubresourceMetadataSpan;
	return mSubresourceMetadataPayloads[metadataSpan.Begin + subresourceIndex].Layout;
}

VkImageUsageFlags Vulkan::FrameGraphBuilder::GetRegisteredSubresourceUsage(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mTotalPassMetadatas[passIndex].SubresourceMetadataSpan;
	return mSubresourceMetadataPayloads[metadataSpan.Begin + subresourceIndex].Usage;
}

VkPipelineStageFlags Vulkan::FrameGraphBuilder::GetRegisteredSubresourceStageFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mTotalPassMetadatas[passIndex].SubresourceMetadataSpan;
	return mSubresourceMetadataPayloads[metadataSpan.Begin + subresourceIndex].Stage;
}

VkImageAspectFlags Vulkan::FrameGraphBuilder::GetRegisteredSubresourceAspectFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mTotalPassMetadatas[passIndex].SubresourceMetadataSpan;
	return mSubresourceMetadataPayloads[metadataSpan.Begin + subresourceIndex].Aspect;
}

VkAccessFlags Vulkan::FrameGraphBuilder::GetRegisteredSubresourceAccessFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mTotalPassMetadatas[passIndex].SubresourceMetadataSpan;
	return mSubresourceMetadataPayloads[metadataSpan.Begin + subresourceIndex].Access;
}

VkImageLayout Vulkan::FrameGraphBuilder::GetPreviousPassSubresourceLayout(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mTotalPassMetadatas[passIndex].SubresourceMetadataSpan;
	uint32_t prevNodeIndex = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex].PrevPassNodeIndex;
	return mSubresourceMetadataPayloads[prevNodeIndex].Layout;
}

VkImageUsageFlags Vulkan::FrameGraphBuilder::GetPreviousPassSubresourceUsage(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mTotalPassMetadatas[passIndex].SubresourceMetadataSpan;
	uint32_t prevNodeIndex = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex].PrevPassNodeIndex;
	return mSubresourceMetadataPayloads[prevNodeIndex].Usage;
}

VkPipelineStageFlags Vulkan::FrameGraphBuilder::GetPreviousPassSubresourceStageFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mTotalPassMetadatas[passIndex].SubresourceMetadataSpan;
	uint32_t prevNodeIndex = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex].PrevPassNodeIndex;
	return mSubresourceMetadataPayloads[prevNodeIndex].Stage;
}

VkImageAspectFlags Vulkan::FrameGraphBuilder::GetPreviousPassSubresourceAspectFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mTotalPassMetadatas[passIndex].SubresourceMetadataSpan;
	uint32_t prevNodeIndex = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex].PrevPassNodeIndex;
	return mSubresourceMetadataPayloads[prevNodeIndex].Aspect;
}

VkAccessFlags Vulkan::FrameGraphBuilder::GetPreviousPassSubresourceAccessFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mTotalPassMetadatas[passIndex].SubresourceMetadataSpan;
	uint32_t prevNodeIndex = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex].PrevPassNodeIndex;
	return mSubresourceMetadataPayloads[prevNodeIndex].Access;
}

VkImageLayout Vulkan::FrameGraphBuilder::GetNextPassSubresourceLayout(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mTotalPassMetadatas[passIndex].SubresourceMetadataSpan;
	uint32_t nextNodeIndex = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex].NextPassNodeIndex;
	return mSubresourceMetadataPayloads[nextNodeIndex].Layout;
}

VkImageUsageFlags Vulkan::FrameGraphBuilder::GetNextPassSubresourceUsage(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mTotalPassMetadatas[passIndex].SubresourceMetadataSpan;
	uint32_t nextNodeIndex = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex].NextPassNodeIndex;
	return mSubresourceMetadataPayloads[nextNodeIndex].Usage;
}

VkPipelineStageFlags Vulkan::FrameGraphBuilder::GetNextPassSubresourceStageFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mTotalPassMetadatas[passIndex].SubresourceMetadataSpan;
	uint32_t nextNodeIndex = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex].NextPassNodeIndex;
	return mSubresourceMetadataPayloads[nextNodeIndex].Stage;
}

VkImageAspectFlags Vulkan::FrameGraphBuilder::GetNextPassSubresourceAspectFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mTotalPassMetadatas[passIndex].SubresourceMetadataSpan;
	uint32_t nextNodeIndex = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex].NextPassNodeIndex;
	return mSubresourceMetadataPayloads[nextNodeIndex].Aspect;
}

VkAccessFlags Vulkan::FrameGraphBuilder::GetNextPassSubresourceAccessFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mTotalPassMetadatas[passIndex].SubresourceMetadataSpan;
	uint32_t nextNodeIndex = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex].NextPassNodeIndex;
	return mSubresourceMetadataPayloads[nextNodeIndex].Access;
}

void Vulkan::FrameGraphBuilder::Build(FrameGraphDescription&& frameGraphDescription, const FrameGraphBuildInfo& buildInfo)
{
	mInstanceParameters   = buildInfo.InstanceParams;
	mDeviceParameters     = buildInfo.DeviceParams;
	mMemoryManager        = buildInfo.MemoryAllocator;
	mDeviceQueues         = buildInfo.Queues;
	mWorkerCommandBuffers = buildInfo.CommandBuffers;

	ModernFrameGraphBuilder::Build(std::move(frameGraphDescription));
}

void Vulkan::FrameGraphBuilder::ValidateDescriptors(DescriptorDatabase* descriptorDatabase, SharedDescriptorDatabaseBuilder* sharedDatabaseBuilder, PassDescriptorDatabaseBuilder* passDatabaseBuilder)
{
	descriptorDatabase->ClearDatabase();

	mShaderDatabase->FlushDescriptorSetData(sharedDatabaseBuilder, passDatabaseBuilder);
	for(uint32_t passIndex = mRenderPassMetadataSpan.Begin; passIndex < mRenderPassMetadataSpan.End; passIndex++)
	{
		mVulkanGraphToBuild->mRenderPasses[passIndex]->ValidateDescriptorSetSpans(descriptorDatabase, mShaderDatabase->GetOriginalDescriptorSpanStart());
	}
}

void Vulkan::FrameGraphBuilder::InitMetadataPayloads()
{
	mSubresourceMetadataPayloads.resize(mSubresourceMetadataNodesFlat.size(), SubresourceMetadataPayload());

	for(uint32_t passIndex = mRenderPassMetadataSpan.Begin; passIndex < mRenderPassMetadataSpan.End; passIndex++)
	{
		const PassMetadata& passMetadata = mTotalPassMetadatas[passIndex];
		mShaderDatabase->RegisterPass(passMetadata.Type);

		Span<uint32_t> passMetadataSpan = passMetadata.SubresourceMetadataSpan;
		std::span<SubresourceMetadataPayload> payloadSpan = {mSubresourceMetadataPayloads.begin() + passMetadataSpan.Begin, mSubresourceMetadataPayloads.begin() + passMetadataSpan.End};

		RegisterPassSubresources(passMetadata.Type, payloadSpan);
		RegisterPassShaders(passMetadata.Type, mShaderDatabase.get());
	}

	for(uint32_t presentPassIndex = mPresentPassMetadataSpan.Begin; presentPassIndex < mPresentPassMetadataSpan.End; presentPassIndex++)
	{
		const PassMetadata& passMetadata = mTotalPassMetadatas[presentPassIndex];

		uint32_t backbufferPayloadIndex = passMetadata.SubresourceMetadataSpan.Begin + (uint32_t)PresentPassSubresourceId::Backbuffer;
		mSubresourceMetadataPayloads[backbufferPayloadIndex].Format         = mSwapChain->GetBackbufferFormat();
		mSubresourceMetadataPayloads[backbufferPayloadIndex].Aspect         = VK_IMAGE_ASPECT_COLOR_BIT;
		mSubresourceMetadataPayloads[backbufferPayloadIndex].Layout         = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		mSubresourceMetadataPayloads[backbufferPayloadIndex].Usage          = 0;
		mSubresourceMetadataPayloads[backbufferPayloadIndex].Stage          = VK_PIPELINE_STAGE_NONE_KHR;
		mSubresourceMetadataPayloads[backbufferPayloadIndex].Access         = 0;
		mSubresourceMetadataPayloads[backbufferPayloadIndex].ImageViewIndex = (uint32_t)(-1);
		mSubresourceMetadataPayloads[backbufferPayloadIndex].Flags          = 0;
	}

	for(uint32_t primarySubresourceSpanIndex = mPrimarySubresourceNodeSpan.Begin; primarySubresourceSpanIndex < mPrimarySubresourceNodeSpan.End; primarySubresourceSpanIndex++)
	{
		Span<uint32_t> helperSpan = mHelperNodeSpansPerPassSubresource[primarySubresourceSpanIndex];
		if(helperSpan.Begin != helperSpan.End)
		{
			std::fill(mSubresourceMetadataPayloads.begin() + helperSpan.Begin, mSubresourceMetadataPayloads.begin() + helperSpan.End, mSubresourceMetadataPayloads[primarySubresourceSpanIndex]);
		}
	}
}

bool Vulkan::FrameGraphBuilder::PropagateSubresourcePayloadDataVertically(const ResourceMetadata& resourceMetadata)
{
	bool propagationHappened = false;

	uint32_t headNodeIndex = resourceMetadata.HeadNodeIndex;
	uint32_t currNodeIndex = headNodeIndex;

	do
	{
		const SubresourceMetadataNode& subresourceNode = mSubresourceMetadataNodesFlat[currNodeIndex];
		const SubresourceMetadataNode& nextSubresourceNode = mSubresourceMetadataNodesFlat[currNodeIndex];
			
		SubresourceMetadataPayload& currSubresourcePayload = mSubresourceMetadataPayloads[currNodeIndex];
		SubresourceMetadataPayload& nextSubresourcePayload = mSubresourceMetadataPayloads[subresourceNode.NextPassNodeIndex];

		//Aspect gets propagated from the current pass to the next pass
		if(nextSubresourcePayload.Aspect == 0 && currSubresourcePayload.Aspect != 0)
		{
			nextSubresourcePayload.Aspect = currSubresourcePayload.Aspect;
			propagationHappened = true;
		}

		//Format gets propagated from the current pass to the next pass
		if(nextSubresourcePayload.Format == VK_FORMAT_UNDEFINED && currSubresourcePayload.Format != VK_FORMAT_UNDEFINED)
		{
			nextSubresourcePayload.Format = currSubresourcePayload.Format;
			propagationHappened = true;
		}

		//Access flags get propagated from top to bottom within a queue for the same layout
		if(PassClassToQueueIndex(nextSubresourceNode.PassClass) == PassClassToQueueIndex(subresourceNode.PassClass) && nextSubresourcePayload.Layout == currSubresourcePayload.Layout && nextSubresourcePayload.Access != currSubresourcePayload.Access)
		{
			nextSubresourcePayload.Access |= currSubresourcePayload.Access;
			currSubresourcePayload.Access |= nextSubresourcePayload.Access;

			propagationHappened = true;
		}

		currNodeIndex = subresourceNode.NextPassNodeIndex;

	} while (headNodeIndex != currNodeIndex);

	return propagationHappened;
}

bool Vulkan::FrameGraphBuilder::PropagateSubresourcePayloadDataHorizontally(const PassMetadata& passMetadata)
{
	Span<uint32_t> subresourceSpan = passMetadata.SubresourceMetadataSpan;

	std::span<SubresourceMetadataPayload> subresourcePayloads = {mSubresourceMetadataPayloads.begin() + subresourceSpan.Begin, mSubresourceMetadataPayloads.begin() + subresourceSpan.End};
	return PropagateSubresourceInfos(passMetadata.Type, subresourcePayloads);
}

void Vulkan::FrameGraphBuilder::CreateTextures()
{
	mVulkanGraphToBuild->mImages.resize(mResourceMetadatas.size());
	mVulkanGraphToBuild->mOwnedImageSpans.clear();


	std::vector<VkBindImageMemoryInfo> bindImageMemoryInfos;
	std::vector<VkImageMemoryBarrier> imageInitialStateBarriers(mResourceMetadatas.size()); //Barriers to initialize images

	uint32_t nextBackbufferImageIndex = 0;
	TextureSourceType lastSourceType = TextureSourceType::Backbuffer;
	for(uint32_t resourceMetadataIndex = 0; resourceMetadataIndex < mResourceMetadatas.size(); resourceMetadataIndex++)
	{
		ResourceMetadata& textureMetadata = mResourceMetadatas[resourceMetadataIndex];

		if(textureMetadata.SourceType == TextureSourceType::PassTexture)
		{
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

			uint8_t formatChanges = 0;

			uint32_t headNodeIndex    = textureMetadata.HeadNodeIndex;
			uint32_t currentNodeIndex = headNodeIndex;
			do
			{
				const SubresourceMetadataNode&    subresourceMetadata        = mSubresourceMetadataNodesFlat[currentNodeIndex];
				const SubresourceMetadataPayload& subresourceMetadataPayload = mSubresourceMetadataPayloads[currentNodeIndex];

				imageCreateInfo.usage |= subresourceMetadataPayload.Usage;

				//TODO: mutable format lists
				assert(subresourceMetadataPayload.Format != VK_FORMAT_UNDEFINED);
				if(imageCreateInfo.format != subresourceMetadataPayload.Format)
				{
					assert(formatChanges < 255);
					formatChanges++;

					imageCreateInfo.format = subresourceMetadataPayload.Format;
				}

				assert(subresourceMetadataPayload.Layout != VK_IMAGE_LAYOUT_UNDEFINED);

				currentNodeIndex = subresourceMetadata.NextPassNodeIndex;

			} while(currentNodeIndex != headNodeIndex);


			if(formatChanges > 1)
			{
				imageCreateInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
			}

			VkImage image = VK_NULL_HANDLE;
			ThrowIfFailed(vkCreateImage(mVulkanGraphToBuild->mDeviceRef, &imageCreateInfo, nullptr, &image));
			mVulkanGraphToBuild->mImages[resourceMetadataIndex] = image;

			bindImageMemoryInfos.push_back(VkBindImageMemoryInfo
			{
				.sType        = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
				.pNext        = nullptr,
				.image        = image,
				.memory       = VK_NULL_HANDLE,
				.memoryOffset = 0
			});

			if(lastSourceType != TextureSourceType::PassTexture)
			{
				mVulkanGraphToBuild->mOwnedImageSpans.push_back(Span<uint32_t>
				{
					.Begin = resourceMetadataIndex,
					.End   = resourceMetadataIndex + 1
				});
			}
			else
			{
				mVulkanGraphToBuild->mOwnedImageSpans.back().End += 1;
			}
		}
		else if(textureMetadata.SourceType == TextureSourceType::Backbuffer)
		{
			mVulkanGraphToBuild->mImages[resourceMetadataIndex] = mSwapChain->GetSwapchainImage(nextBackbufferImageIndex);
			nextBackbufferImageIndex++;
		}

		lastSourceType = textureMetadata.SourceType;


		uint32_t lastSubresourceNodeIndex = mSubresourceMetadataNodesFlat[textureMetadata.HeadNodeIndex].PrevPassNodeIndex;
		const SubresourceMetadataNode&    lastSubresourceMetadata = mSubresourceMetadataNodesFlat[lastSubresourceNodeIndex];
		const SubresourceMetadataPayload& lastSubresourcePayload  = mSubresourceMetadataPayloads[lastSubresourceNodeIndex];

		VkImageMemoryBarrier imageInitialBarrier;
		imageInitialBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageInitialBarrier.pNext                           = nullptr;
		imageInitialBarrier.srcAccessMask                   = 0;
		imageInitialBarrier.dstAccessMask                   = lastSubresourcePayload.Access;
		imageInitialBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInitialBarrier.newLayout                       = lastSubresourcePayload.Layout;
		imageInitialBarrier.srcQueueFamilyIndex             = mDeviceQueues->GetGraphicsQueueFamilyIndex(); //Resources were created with that queue ownership
		imageInitialBarrier.dstQueueFamilyIndex             = PassClassToQueueIndex(lastSubresourceMetadata.PassClass);
		imageInitialBarrier.image                           = mVulkanGraphToBuild->mImages[resourceMetadataIndex];
		imageInitialBarrier.subresourceRange.aspectMask     = lastSubresourcePayload.Aspect;
		imageInitialBarrier.subresourceRange.baseMipLevel   = 0;
		imageInitialBarrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
		imageInitialBarrier.subresourceRange.baseArrayLayer = 0;
		imageInitialBarrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;
		
		imageInitialStateBarriers[resourceMetadataIndex] = imageInitialBarrier;

#if defined(DEBUG) || defined(_DEBUG)
		if(mInstanceParameters->IsDebugUtilsExtensionEnabled())
		{
			VulkanUtils::SetDebugObjectName(mVulkanGraphToBuild->mDeviceRef, mVulkanGraphToBuild->mImages[resourceMetadataIndex], textureMetadata.Name);
		}
#endif
	}


	//Allocate the memory
	SafeDestroyObject(vkFreeMemory, mVulkanGraphToBuild->mDeviceRef, mVulkanGraphToBuild->mImageMemory);
	mVulkanGraphToBuild->mImageMemory = mMemoryManager->AllocateImagesMemory(mVulkanGraphToBuild->mDeviceRef, bindImageMemoryInfos);

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

void Vulkan::FrameGraphBuilder::CreateTextureViews()
{
	mVulkanGraphToBuild->mImageViews.clear();
	for(uint32_t resourceMetadataIndex = 0; resourceMetadataIndex < mResourceMetadatas.size(); resourceMetadataIndex++)
	{
		const ResourceMetadata& resourceMetadata = mResourceMetadatas[resourceMetadataIndex];

		std::unordered_map<uint64_t, uint32_t> imageViewIndicesForViewInfos; //To only create different image views if the format + aspect flags differ

		uint32_t headNodeIndex = resourceMetadata.HeadNodeIndex;
		uint32_t currNodeIndex = headNodeIndex;
		do
		{
			const SubresourceMetadataNode& subresourceMetadata     = mSubresourceMetadataNodesFlat[currNodeIndex];
			SubresourceMetadataPayload& subresourceMetadataPayload = mSubresourceMetadataPayloads[currNodeIndex];

			if(currNodeIndex < mPrimarySubresourceNodeSpan.Begin || currNodeIndex >= mPrimarySubresourceNodeSpan.End)
			{
				//Helper subresources don't create views
				currNodeIndex = subresourceMetadata.NextPassNodeIndex;
				continue;
			}

			VkImageUsageFlags imageViewUsages = (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT 
				                               | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT 
				                               | VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT | VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR);

			if(subresourceMetadataPayload.Usage & imageViewUsages)
			{
				uint64_t viewInfoKey = ((uint64_t)subresourceMetadataPayload.Aspect << 32ull) | (subresourceMetadataPayload.Format);
			
				auto imageViewIt = imageViewIndicesForViewInfos.find(viewInfoKey);
				if(imageViewIt != imageViewIndicesForViewInfos.end())
				{
					subresourceMetadataPayload.ImageViewIndex = imageViewIt->second;
				}
				else
				{
					uint32_t newImageViewIndex = (uint32_t)mVulkanGraphToBuild->mImageViews.size();

					VkImage image = mVulkanGraphToBuild->mImages[resourceMetadataIndex];
					mVulkanGraphToBuild->mImageViews.push_back(CreateImageView(image, subresourceMetadataPayload.Format, subresourceMetadataPayload.Aspect));

					subresourceMetadataPayload.ImageViewIndex = newImageViewIndex;
					imageViewIndicesForViewInfos[viewInfoKey] = newImageViewIndex;
				}
			}

			currNodeIndex = subresourceMetadata.NextPassNodeIndex;

		} while(currNodeIndex != headNodeIndex);
	}
}

void Vulkan::FrameGraphBuilder::BuildPassObjects()
{
	mShaderDatabase->BuildSetLayouts(mDeviceParameters);
	for(uint32_t passIndex = mRenderPassMetadataSpan.Begin; passIndex < mRenderPassMetadataSpan.End; passIndex++)
	{
		mVulkanGraphToBuild->mRenderPasses.emplace_back(MakeUniquePass(mTotalPassMetadatas[passIndex].Type, this, passIndex));
	}
}

void Vulkan::FrameGraphBuilder::CreateBeforePassBarriers(const PassMetadata& passMetadata, uint32_t barrierSpanIndex)
{
	mVulkanGraphToBuild->mRenderPassBarriers[barrierSpanIndex].BeforePassBegin = (uint32_t)mVulkanGraphToBuild->mImageBarriers.size();

	for(uint32_t metadataIndex = passMetadata.SubresourceMetadataSpan.Begin; metadataIndex < passMetadata.SubresourceMetadataSpan.End; metadataIndex++)
	{
		const SubresourceMetadataNode& currMetadataNode = mSubresourceMetadataNodesFlat[metadataIndex];
		const SubresourceMetadataNode& prevMetadataNode = mSubresourceMetadataNodesFlat[currMetadataNode.PrevPassNodeIndex];

		const SubresourceMetadataPayload& currMetadataPayload = mSubresourceMetadataPayloads[metadataIndex];
		const SubresourceMetadataPayload& prevMetadataPayload = mSubresourceMetadataPayloads[currMetadataNode.PrevPassNodeIndex];

		if(!(currMetadataPayload.Flags & TextureFlagAutoBeforeBarrier) && !(prevMetadataPayload.Flags & TextureFlagAutoAfterBarrier))
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

			bool barrierNeeded = false;
			VkAccessFlags prevAccessMask = prevMetadataPayload.Access;
			VkAccessFlags currAccessMask = currMetadataPayload.Access;

			if(PassClassToQueueIndex(prevMetadataNode.PassClass) == PassClassToQueueIndex(currMetadataNode.PassClass)) //Rules 1, 2, 3
			{
				barrierNeeded = (currMetadataPayload.Layout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) && (prevMetadataPayload.Layout != currMetadataPayload.Layout);
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
				imageMemoryBarrier.oldLayout                       = prevMetadataPayload.Layout;
				imageMemoryBarrier.newLayout                       = currMetadataPayload.Layout;
				imageMemoryBarrier.srcQueueFamilyIndex             = PassClassToQueueIndex(prevMetadataNode.PassClass);
				imageMemoryBarrier.dstQueueFamilyIndex             = PassClassToQueueIndex(currMetadataNode.PassClass);
				imageMemoryBarrier.image                           = mVulkanGraphToBuild->mImages[currMetadataNode.ResourceMetadataIndex];
				imageMemoryBarrier.subresourceRange.aspectMask     = prevMetadataPayload.Aspect | currMetadataPayload.Aspect;
				imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
				imageMemoryBarrier.subresourceRange.layerCount     = 1;
				imageMemoryBarrier.subresourceRange.baseMipLevel   = 0;
				imageMemoryBarrier.subresourceRange.levelCount     = 1;

				mVulkanGraphToBuild->mImageBarriers.push_back(imageMemoryBarrier);
			}
		}
	}

	mVulkanGraphToBuild->mRenderPassBarriers[barrierSpanIndex].BeforePassEnd = (uint32_t)mVulkanGraphToBuild->mImageBarriers.size();
}

void Vulkan::FrameGraphBuilder::CreateAfterPassBarriers(const PassMetadata& passMetadata, uint32_t barrierSpanIndex)
{
	mVulkanGraphToBuild->mRenderPassBarriers[barrierSpanIndex].AfterPassBegin = (uint32_t)mVulkanGraphToBuild->mImageBarriers.size();

	for(uint32_t metadataIndex = passMetadata.SubresourceMetadataSpan.Begin; metadataIndex < passMetadata.SubresourceMetadataSpan.End; metadataIndex++)
	{
		const SubresourceMetadataNode& currMetadataNode = mSubresourceMetadataNodesFlat[metadataIndex];
		const SubresourceMetadataNode& nextMetadataNode = mSubresourceMetadataNodesFlat[currMetadataNode.NextPassNodeIndex];

		const SubresourceMetadataPayload& currMetadataPayload = mSubresourceMetadataPayloads[metadataIndex];
		const SubresourceMetadataPayload& nextMetadataPayload = mSubresourceMetadataPayloads[currMetadataNode.NextPassNodeIndex];

		if(!(currMetadataPayload.Flags & TextureFlagAutoAfterBarrier) && !(nextMetadataPayload.Flags & TextureFlagAutoBeforeBarrier))
		{
			/*
			*    1. Same queue family, layout unchanged:           No barrier needed
			*    2. Same queue family, layout changed to PRESENT:  Need a barrier Old layout -> New Layout
			*    3. Same queue family, layout changed other cases: No barrier needed, will be handled by the next state's barrier
			*
			*    4. Queue family changed, layout unchanged: Need a release barrier with old access mask
			*    5. Queue family changed, layout changed:   Need a release + layout change barrier
			*    6. Queue family changed, from present:     Need a release barrier with source and destination access masks 0
			*    7. Queue family changed, to present:       Need a release barrier with old access mask
			*/

			bool barrierNeeded = false;
			VkAccessFlags currAccessMask = currMetadataPayload.Access;
			VkAccessFlags nextAccessMask = nextMetadataPayload.Access;
			if(PassClassToQueueIndex(currMetadataNode.PassClass) == PassClassToQueueIndex(nextMetadataNode.PassClass)) //Rules 1, 2, 3
			{
				barrierNeeded = (nextMetadataPayload.Layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
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
				imageMemoryBarrier.oldLayout                       = currMetadataPayload.Layout;
				imageMemoryBarrier.newLayout                       = nextMetadataPayload.Layout;
				imageMemoryBarrier.srcQueueFamilyIndex             = PassClassToQueueIndex(currMetadataNode.PassClass);
				imageMemoryBarrier.dstQueueFamilyIndex             = PassClassToQueueIndex(nextMetadataNode.PassClass);
				imageMemoryBarrier.image                           = mVulkanGraphToBuild->mImages[currMetadataNode.ResourceMetadataIndex];
				imageMemoryBarrier.subresourceRange.aspectMask     = currMetadataPayload.Aspect | nextMetadataPayload.Aspect;
				imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
				imageMemoryBarrier.subresourceRange.layerCount     = 1;
				imageMemoryBarrier.subresourceRange.baseMipLevel   = 0;
				imageMemoryBarrier.subresourceRange.levelCount     = 1;

				mVulkanGraphToBuild->mImageBarriers.push_back(imageMemoryBarrier);
			}
		}
	}

	mVulkanGraphToBuild->mRenderPassBarriers[barrierSpanIndex].AfterPassEnd = (uint32_t)mVulkanGraphToBuild->mImageBarriers.size();
}

void Vulkan::FrameGraphBuilder::InitializeTraverseData() const
{
	mVulkanGraphToBuild->mFrameRecordedGraphicsCommandBuffers.resize(mVulkanGraphToBuild->mGraphicsPassSpansPerDependencyLevel.size());
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
