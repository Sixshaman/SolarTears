#include "VulkanFrameGraphBuilder.hpp"
#include "VulkanFrameGraph.hpp"
#include "../VulkanInstanceParameters.hpp"
#include "../VulkanWorkerCommandBuffers.hpp"
#include "../VulkanFunctions.hpp"
#include "../VulkanMemory.hpp"
#include "../VulkanSwapChain.hpp"
#include "../VulkanDeviceQueues.hpp"
#include <algorithm>
#include <numeric>

Vulkan::FrameGraphBuilder::FrameGraphBuilder(FrameGraph* graphToBuild, const SwapChain* swapchain): ModernFrameGraphBuilder(graphToBuild), mVulkanGraphToBuild(graphToBuild), mSwapChain(swapchain)
{
	mImageViewCount = 0;
}

Vulkan::FrameGraphBuilder::~FrameGraphBuilder()
{
}

void Vulkan::FrameGraphBuilder::RegisterRenderPass(const std::string_view passName, RenderPassCreateFunc createFunc, RenderPassType passType)
{
	RenderPassName renderPassName(passName);
	assert(!mRenderPassCreateFunctions.contains(renderPassName));

	mRenderPassNames.push_back(renderPassName);
	mRenderPassCreateFunctions[renderPassName] = createFunc;

	mRenderPassTypes[renderPassName] = passType;
}

void Vulkan::FrameGraphBuilder::SetPassSubresourceFormat(const std::string_view passName, const std::string_view subresourceId, VkFormat format)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	mSubresourceInfos[subresourceInfoIndex].Format = format;
}

void Vulkan::FrameGraphBuilder::SetPassSubresourceLayout(const std::string_view passName, const std::string_view subresourceId, VkImageLayout layout)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	mSubresourceInfos[subresourceInfoIndex].Layout = layout;
}

void Vulkan::FrameGraphBuilder::SetPassSubresourceUsage(const std::string_view passName, const std::string_view subresourceId, VkImageUsageFlags usage)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	mSubresourceInfos[subresourceInfoIndex].Usage = usage;
}

void Vulkan::FrameGraphBuilder::SetPassSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId, VkImageAspectFlags aspect)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	mSubresourceInfos[subresourceInfoIndex].Aspect = aspect;
}

void Vulkan::FrameGraphBuilder::SetPassSubresourceStageFlags(const std::string_view passName, const std::string_view subresourceId, VkPipelineStageFlags stage)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	mSubresourceInfos[subresourceInfoIndex].Stage = stage;
}

void Vulkan::FrameGraphBuilder::SetPassSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId, VkAccessFlags accessFlags)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	mSubresourceInfos[subresourceInfoIndex].Access = accessFlags;
}

const Vulkan::DescriptorManager* Vulkan::FrameGraphBuilder::GetDescriptorManager() const
{
	return mDescriptorManager;
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

const Vulkan::ShaderManager* Vulkan::FrameGraphBuilder::GetShaderManager() const
{
	return mShaderManager;
}

VkImage Vulkan::FrameGraphBuilder::GetRegisteredResource(const std::string_view passName, const std::string_view subresourceId, uint32_t frame) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const SubresourceMetadataNode& metadataNode = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
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
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const SubresourceMetadataNode& metadataNode = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
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

VkFormat Vulkan::FrameGraphBuilder::GetRegisteredSubresourceFormat(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].Format;
}

VkImageLayout Vulkan::FrameGraphBuilder::GetRegisteredSubresourceLayout(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].Layout;
}

VkImageUsageFlags Vulkan::FrameGraphBuilder::GetRegisteredSubresourceUsage(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].Usage;
}

VkPipelineStageFlags Vulkan::FrameGraphBuilder::GetRegisteredSubresourceStageFlags(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].Stage;
}

VkImageAspectFlags Vulkan::FrameGraphBuilder::GetRegisteredSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].Aspect;
}

VkAccessFlags Vulkan::FrameGraphBuilder::GetRegisteredSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].Access;
}

VkImageLayout Vulkan::FrameGraphBuilder::GetPreviousPassSubresourceLayout(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).PrevPassNode->SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].Layout;
}

VkImageUsageFlags Vulkan::FrameGraphBuilder::GetPreviousPassSubresourceUsage(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).PrevPassNode->SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].Usage;
}

VkPipelineStageFlags Vulkan::FrameGraphBuilder::GetPreviousPassSubresourceStageFlags(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).PrevPassNode->SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].Stage;
}

VkImageAspectFlags Vulkan::FrameGraphBuilder::GetPreviousPassSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).PrevPassNode->SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].Aspect;
}

VkAccessFlags Vulkan::FrameGraphBuilder::GetPreviousPassSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).PrevPassNode->SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].Access;
}

VkImageLayout Vulkan::FrameGraphBuilder::GetNextPassSubresourceLayout(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).NextPassNode->SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].Layout;
}

VkImageUsageFlags Vulkan::FrameGraphBuilder::GetNextPassSubresourceUsage(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).NextPassNode->SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].Usage;
}

VkPipelineStageFlags Vulkan::FrameGraphBuilder::GetNextPassSubresourceStageFlags(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).NextPassNode->SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].Stage;
}

VkImageAspectFlags Vulkan::FrameGraphBuilder::GetNextPassSubresourceAspectFlags(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).NextPassNode->SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].Aspect;
}

VkAccessFlags Vulkan::FrameGraphBuilder::GetNextPassSubresourceAccessFlags(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).NextPassNode->SubresourceInfoIndex;
	return mSubresourceInfos[subresourceInfoIndex].Access;
}

void Vulkan::FrameGraphBuilder::Build(const InstanceParameters* instanceParameters, const DeviceParameters* deviceParameters, const DescriptorManager* descriptorManager, const MemoryManager* memoryManager, const ShaderManager* shaderManager, const DeviceQueues* deviceQueues, WorkerCommandBuffers* workerCommandBuffers)
{
	mInstanceParameters   = instanceParameters;
	mDeviceParameters     = deviceParameters;
	mDescriptorManager    = descriptorManager;
	mMemoryManager        = memoryManager;
	mShaderManager        = shaderManager;
	mDeviceQueues         = deviceQueues;
	mWorkerCommandBuffers = workerCommandBuffers;

	ModernFrameGraphBuilder::Build();
}

void Vulkan::FrameGraphBuilder::CreateTextures(const std::vector<TextureResourceCreateInfo>& textureCreateInfos, const std::vector<TextureResourceCreateInfo>& backbufferCreateInfos, uint32_t totalTextureCount) const
{
	mVulkanGraphToBuild->mImages.resize(totalTextureCount);
	std::vector<VkImageMemoryBarrier> imageInitialStateBarriers(totalTextureCount); //Barriers to initialize images

	for(const TextureResourceCreateInfo& textureCreateInfo: textureCreateInfos)
	{
		SubresourceMetadataNode* headNode    = textureCreateInfo.MetadataHead;
		SubresourceMetadataNode* currentNode = headNode;

		VkFormat headFormat = mSubresourceInfos[headNode->SubresourceInfoIndex].Format;

		VkImageUsageFlags  usageFlags       = 0;
		VkImageCreateFlags imageCreateFlags = 0;
		std::array         queueIndices     = {mDeviceQueues->GetGraphicsQueueFamilyIndex()};
		do
		{
			const SubresourceInfo& subresourceInfo = mSubresourceInfos[currentNode->SubresourceInfoIndex];

			queueIndices[0] = PassTypeToQueueIndex(currentNode->PassType);
			usageFlags     |= subresourceInfo.Usage;
			
			//TODO: mutable format lists
			if(subresourceInfo.Format != headFormat)
			{
				imageCreateFlags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
			}

		} while(currentNode != headNode);

		uint32_t lastSubresourceInfo   = headNode->PrevPassNode->SubresourceInfoIndex;
		RenderPassType lastPassType    = headNode->PrevPassNode->PassType;
		VkImageLayout  lastLayout      = mSubresourceInfos[lastSubresourceInfo].Layout;
		VkAccessFlags  lastAccessFlags = mSubresourceInfos[lastSubresourceInfo].Access;
		VkAccessFlags  lastAspectMask  = mSubresourceInfos[lastSubresourceInfo].Aspect;


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

		for(uint32_t frame = 0; frame < textureCreateInfo.MetadataHead->FrameCount; frame++)
		{
			VkImage image = VK_NULL_HANDLE;
			ThrowIfFailed(vkCreateImage(mVulkanGraphToBuild->mDeviceRef, &imageCreateInfo, nullptr, &image));

#if defined(DEBUG) || defined(_DEBUG)
			if(mInstanceParameters->IsDebugUtilsExtensionEnabled())
			{
				VulkanUtils::SetDebugObjectName(mVulkanGraphToBuild->mDeviceRef, image, textureCreateInfo.Name);
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
			imageBarrier.dstQueueFamilyIndex             = PassTypeToQueueIndex(lastPassType);
			imageBarrier.image                           = image;
			imageBarrier.subresourceRange.aspectMask     = lastAspectMask;
			imageBarrier.subresourceRange.baseMipLevel   = 0;
			imageBarrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
			imageBarrier.subresourceRange.baseArrayLayer = 0;
			imageBarrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

			mVulkanGraphToBuild->mImages[headNode->FirstFrameHandle + frame] = image;
			imageInitialStateBarriers[headNode->FirstFrameHandle + frame]    = imageBarrier;
		}
	}

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
	vkCmdPipelineBarrier(graphicsCommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
		                 (uint32_t)memoryBarriers.size(),            memoryBarriers.data(),
		                 (uint32_t)bufferBarriers.size(),            bufferBarriers.data(),
		                 (uint32_t)imageInitialStateBarriers.size(), imageInitialStateBarriers.data());

	ThrowIfFailed(vkEndCommandBuffer(graphicsCommandBuffer));

	mDeviceQueues->GraphicsQueueSubmit(graphicsCommandBuffer);


	for(const TextureResourceCreateInfo& backbufferCreateInfo: backbufferCreateInfos)
	{
		for(uint32_t frame = 0; frame < backbufferCreateInfo.MetadataHead->FrameCount; frame++)
		{
			mVulkanGraphToBuild->mImages[backbufferCreateInfo.MetadataHead->FirstFrameHandle + frame] = mSwapChain->GetSwapchainImage(frame);

#if defined(DEBUG) || defined(_DEBUG)
			VulkanUtils::SetDebugObjectName(mVulkanGraphToBuild->mDeviceRef, mVulkanGraphToBuild->mImages.back(), mBackbufferName);
#endif
		}
	}

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
	subresourceInfo.Aspect = 0;
	subresourceInfo.Stage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	subresourceInfo.Access = 0;

	mSubresourceInfos.push_back(subresourceInfo);
	return (uint32_t)(mSubresourceInfos.size() - 1);
}

void Vulkan::FrameGraphBuilder::AddRenderPass(const RenderPassName& passName, uint32_t frame)
{
	mVulkanGraphToBuild->mRenderPasses.push_back(mRenderPassCreateFunctions.at(passName)(mVulkanGraphToBuild->mDeviceRef, this, passName, frame));
}

uint32_t Vulkan::FrameGraphBuilder::NextPassSpanId()
{
	return (uint32_t)mVulkanGraphToBuild->mRenderPasses.size();
}

bool Vulkan::FrameGraphBuilder::ValidateSubresourceViewParameters(SubresourceMetadataNode* node)
{
	SubresourceInfo& prevPassSubresourceInfo = mSubresourceInfos[node->PrevPassNode->SubresourceInfoIndex];
	SubresourceInfo& thisPassSubresourceInfo = mSubresourceInfos[node->SubresourceInfoIndex];

	if(thisPassSubresourceInfo.Format == VK_FORMAT_UNDEFINED && prevPassSubresourceInfo.Format != VK_FORMAT_UNDEFINED)
	{
		thisPassSubresourceInfo.Format = prevPassSubresourceInfo.Format;
	}

	if(thisPassSubresourceInfo.Aspect == 0 && prevPassSubresourceInfo.Aspect != 0)
	{
		thisPassSubresourceInfo.Aspect = prevPassSubresourceInfo.Aspect;
	}

	bool accessFlagsPropagated = false;
	if(node->PassType == node->PrevPassNode->PassType && thisPassSubresourceInfo.Layout == prevPassSubresourceInfo.Layout && thisPassSubresourceInfo.Access != prevPassSubresourceInfo.Access)
	{
		thisPassSubresourceInfo.Access |= prevPassSubresourceInfo.Access;
		prevPassSubresourceInfo.Access |= thisPassSubresourceInfo.Access;

		accessFlagsPropagated = true;
	}

	node->ViewSortKey = ((uint64_t)thisPassSubresourceInfo.Aspect << 32) | (uint32_t)thisPassSubresourceInfo.Format;
	return thisPassSubresourceInfo.Format != 0 && thisPassSubresourceInfo.Aspect != 0 && !accessFlagsPropagated;
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

uint32_t Vulkan::FrameGraphBuilder::AddBeforePassBarrier(uint32_t imageIndex, RenderPassType prevPassType, uint32_t prevPassSubresourceInfoIndex, RenderPassType currPassType, uint32_t currPassSubresourceInfoIndex)
{
	/*
	*    1. Same queue family, layout unchanged: No barrier needed
	*    2. Same queue family, layout changed:   Need a barrier Old layout -> New layout
	*
	*    3. Queue family changed, layout unchanged: Need an acquire barrier with new access mask
	*    4. Queue family changed, layout changed:   Need an acquire + layout change barrier
	*    5. Queue family changed, from present:     Need an acquire barrier with new access mask
	*    6. Queue family changed, to present:       Need an acquire barrier with source and destination access masks 0
	*/

	const SubresourceInfo& prevPassInfo = mSubresourceInfos[prevPassSubresourceInfoIndex];
	const SubresourceInfo& currPassInfo = mSubresourceInfos[currPassSubresourceInfoIndex];

	bool barrierNeeded = false;
	VkAccessFlags prevAccessMask = prevPassInfo.Access;
	VkAccessFlags currAccessMask = currPassInfo.Access;

	if(prevPassType == currPassType) //Rules 1, 2
	{
		barrierNeeded = (prevPassInfo.Layout != currPassInfo.Layout);
	}
	else //Rules 3, 4, 5, 6
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
		imageMemoryBarrier.srcQueueFamilyIndex             = PassTypeToQueueIndex(prevPassType);
		imageMemoryBarrier.dstQueueFamilyIndex             = PassTypeToQueueIndex(currPassType);
		imageMemoryBarrier.image                           = mVulkanGraphToBuild->mImages[imageIndex];
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

uint32_t Vulkan::FrameGraphBuilder::AddAfterPassBarrier(uint32_t imageIndex, RenderPassType currPassType, uint32_t currPassSubresourceInfoIndex, RenderPassType nextPassType, uint32_t nextPassSubresourceInfoIndex)
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

	const SubresourceInfo& currPassInfo = mSubresourceInfos[currPassSubresourceInfoIndex];
	const SubresourceInfo& nextPassInfo = mSubresourceInfos[nextPassSubresourceInfoIndex];

	bool barrierNeeded = false;
	VkAccessFlags currAccessMask = currPassInfo.Access;
	VkAccessFlags nextAccessMask = nextPassInfo.Access;
	if(currPassType == nextPassType) //Rules 1, 2, 3
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
		imageMemoryBarrier.srcQueueFamilyIndex             = PassTypeToQueueIndex(currPassType);
		imageMemoryBarrier.dstQueueFamilyIndex             = PassTypeToQueueIndex(nextPassType);
		imageMemoryBarrier.image                           = mVulkanGraphToBuild->mImages[imageIndex];
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

uint32_t Vulkan::FrameGraphBuilder::PassTypeToQueueIndex(RenderPassType passType) const
{
	switch(passType)
	{
	case RenderPassType::Graphics:
		return mDeviceQueues->GetGraphicsQueueFamilyIndex();

	case RenderPassType::Compute:
		return mDeviceQueues->GetComputeQueueFamilyIndex();

	case RenderPassType::Transfer:
		return mDeviceQueues->GetTransferQueueFamilyIndex();

	case RenderPassType::Present:
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
