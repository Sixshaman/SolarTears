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

Vulkan::FrameGraphBuilder::FrameGraphBuilder(FrameGraph* graphToBuild, const DescriptorManager* descriptorManager, 
	                                         const InstanceParameters* instanceParameters, const DeviceParameters* deviceParameters, 
	                                         const ShaderManager* shaderManager, const MemoryManager* memoryManager, 
	                                         const DeviceQueues* deviceQueues, const SwapChain* swapchain): ModernFrameGraphBuilder(graphToBuild), mVulkanGraphToBuild(graphToBuild),
	                                                                                                        mDescriptorManager(descriptorManager), mInstanceParameters(instanceParameters), mDeviceParameters(deviceParameters), 
	                                                                                                        mShaderManager(shaderManager), mMemoryManager(memoryManager), mDeviceQueues(deviceQueues), mSwapChain(swapchain)
{
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

void Vulkan::FrameGraphBuilder::EnableSubresourceAutoBarrier(const std::string_view passName, const std::string_view subresourceId, bool autoBaarrier)
{
	RenderPassName passNameStr(passName);
	SubresourceId  subresourceIdStr(subresourceId);

	uint32_t subresourceInfoIndex = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr).SubresourceInfoIndex;
	if(autoBaarrier)
	{
		mSubresourceInfos[subresourceInfoIndex].Flags |= ImageFlagAutoBarrier;
	}
	else
	{
		mSubresourceInfos[subresourceInfoIndex].Flags &= ~ImageFlagAutoBarrier;
	}
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

const Vulkan::MemoryManager* Vulkan::FrameGraphBuilder::GetMemoryManager() const
{
	return mMemoryManager;
}

const Vulkan::ShaderManager* Vulkan::FrameGraphBuilder::GetShaderManager() const
{
	return mShaderManager;
}

VkImage Vulkan::FrameGraphBuilder::GetRegisteredResource(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const SubresourceMetadataNode& metadataNode = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadataNode.ImageIndex == (uint32_t)(-1))
	{
		return VK_NULL_HANDLE;
	}
	else
	{
		return mVulkanGraphToBuild->mImages[metadataNode.ImageIndex];
	}
}

VkImageView Vulkan::FrameGraphBuilder::GetRegisteredSubresource(const std::string_view passName, const std::string_view subresourceId) const
{
	const RenderPassName passNameStr(passName);
	const SubresourceId  subresourceIdStr(subresourceId);

	const SubresourceMetadataNode& metadataNode = mRenderPassesSubresourceMetadatas.at(passNameStr).at(subresourceIdStr);
	if(metadataNode.ImageViewIndex == (uint32_t)(-1))
	{
		return VK_NULL_HANDLE;
	}
	else
	{
		return mVulkanGraphToBuild->mImageViews[metadataNode.ImageViewIndex];
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

void Vulkan::FrameGraphBuilder::BuildPassObjects(const std::unordered_set<std::string>& swapchainPassNames)
{
	mGraphToBuild->mRenderPasses.clear();
	mGraphToBuild->mSwapchainRenderPasses.clear();
	mGraphToBuild->mSwapchainPassesSwapMap.clear();

	for(const std::string& passName: mRenderPassNames)
	{
		if(swapchainPassNames.contains(passName))
		{
			//This pass uses swapchain images - create a copy of render pass for each swapchain image
			mGraphToBuild->mRenderPasses.emplace_back(nullptr);

			//Fill in primary swap link (what to swap)
			mGraphToBuild->mSwapchainPassesSwapMap.push_back((uint32_t)(mGraphToBuild->mRenderPasses.size() - 1));

			//Correctness is guaranteed as long as mLastSwapchainImageIndex is the last index in mSwapchainImageRefs
			const uint32_t swapchainImageCount = (uint32_t)mGraphToBuild->mSwapchainImageRefs.size();			
			for(uint32_t swapchainImageIndex = 0; swapchainImageIndex < mGraphToBuild->mSwapchainImageRefs.size(); swapchainImageIndex++)
			{
				//Create a separate render pass for each of swapchain images
				mGraphToBuild->SwitchSwapchainImages(swapchainImageIndex);
				mGraphToBuild->mSwapchainRenderPasses.push_back(mRenderPassCreateFunctions.at(passName)(mGraphToBuild->mDeviceRef, this, passName));

				//Fill in secondary swap link (what to swap to)
				mGraphToBuild->mSwapchainPassesSwapMap.push_back((uint32_t)(mGraphToBuild->mSwapchainRenderPasses.size() - 1));

				mGraphToBuild->mLastSwapchainImageIndex = swapchainImageIndex;
			}
		}
		else
		{
			//This pass does not use any swapchain images - can just create a single copy
			mGraphToBuild->mRenderPasses.push_back(mRenderPassCreateFunctions.at(passName)(mGraphToBuild->mDeviceRef, this, passName));
		}
	}

	//Prepare for the first use
	uint32_t swapchainImageCount = (uint32_t)mGraphToBuild->mSwapchainImageRefs.size();
	for(uint32_t i = 0; i < (uint32_t)mGraphToBuild->mSwapchainPassesSwapMap.size(); i += (swapchainImageCount + 1u))
	{
		uint32_t passIndexToSwitch = mGraphToBuild->mSwapchainPassesSwapMap[i];
		mGraphToBuild->mRenderPasses[passIndexToSwitch].swap(mGraphToBuild->mSwapchainRenderPasses[mGraphToBuild->mSwapchainPassesSwapMap[i + mGraphToBuild->mLastSwapchainImageIndex + 1]]);
	}
}

void Vulkan::FrameGraphBuilder::BuildBarriers()
{
	mGraphToBuild->mImageRenderPassBarriers.resize(mRenderPassNames.size());
	mGraphToBuild->mSwapchainBarrierIndices.clear();

	std::unordered_set<uint64_t> processedMetadataIds; //This is here so no barrier gets added twice
	for(size_t i = 0; i < mRenderPassNames.size(); i++)
	{
		RenderPassName renderPassName = mRenderPassNames[i];

		const auto& nameIdMap     = mRenderPassesSubresourceNameIds.at(renderPassName);
		const auto& idMetadataMap = mRenderPassesSubresourceMetadatas.at(renderPassName);

		std::vector<VkImageMemoryBarrier> beforeBarriers;
		std::vector<VkImageMemoryBarrier> afterBarriers;
		std::vector<size_t>               beforeSwapchainBarriers;
		std::vector<size_t>               afterSwapchainBarriers;
		for(auto& subresourceNameId: nameIdMap)
		{
			SubresourceName subresourceName = subresourceNameId.first;
			SubresourceId   subresourceId   = subresourceNameId.second;

			ImageSubresourceMetadata subresourceMetadata = idMetadataMap.at(subresourceId);
			if(!(subresourceMetadata.MetadataFlags & ImageFlagAutoBarrier))
			{
				if(subresourceMetadata.PrevPassMetadata != nullptr)
				{
					if(processedMetadataIds.contains(subresourceMetadata.PrevPassMetadata->MetadataId)) //Was already processed as "after pass"
					{
						continue;
					}

					const bool accessFlagsDiffer   = (subresourceMetadata.AccessFlags          != subresourceMetadata.PrevPassMetadata->AccessFlags);
					const bool layoutsDiffer       = (subresourceMetadata.Layout               != subresourceMetadata.PrevPassMetadata->Layout);
					const bool queueFamiliesDiffer = (subresourceMetadata.QueueFamilyOwnership != subresourceMetadata.PrevPassMetadata->QueueFamilyOwnership);
					if(accessFlagsDiffer || layoutsDiffer || queueFamiliesDiffer)
					{
						//Swapchain image barriers are changed every frame
						VkImage barrieredImage = nullptr;
						if(subresourceMetadata.ImageIndex != mGraphToBuild->mBackbufferRefIndex)
						{
							barrieredImage = mGraphToBuild->mImages[subresourceMetadata.ImageIndex];
						}

						VkImageMemoryBarrier imageMemoryBarrier;
						imageMemoryBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
						imageMemoryBarrier.pNext                           = nullptr;
						imageMemoryBarrier.srcAccessMask                   = subresourceMetadata.PrevPassMetadata->AccessFlags;
						imageMemoryBarrier.dstAccessMask                   = subresourceMetadata.AccessFlags;
						imageMemoryBarrier.oldLayout                       = subresourceMetadata.PrevPassMetadata->Layout;
						imageMemoryBarrier.newLayout                       = subresourceMetadata.Layout;
						imageMemoryBarrier.srcQueueFamilyIndex             = subresourceMetadata.PrevPassMetadata->QueueFamilyOwnership;
						imageMemoryBarrier.dstQueueFamilyIndex             = subresourceMetadata.QueueFamilyOwnership;
						imageMemoryBarrier.image                           = barrieredImage;
						imageMemoryBarrier.subresourceRange.aspectMask     = subresourceMetadata.AspectFlags;
						imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
						imageMemoryBarrier.subresourceRange.layerCount     = 1;
						imageMemoryBarrier.subresourceRange.baseMipLevel   = 0;
						imageMemoryBarrier.subresourceRange.levelCount     = 1;

						beforeBarriers.push_back(imageMemoryBarrier);

						//Mark the swapchain barriers
						if(barrieredImage == nullptr)
						{
							beforeSwapchainBarriers.push_back(beforeBarriers.size() - 1);
						}
					}
				}

				if(subresourceMetadata.NextPassMetadata != nullptr)
				{
					const bool accessFlagsDiffer   = (subresourceMetadata.AccessFlags          != subresourceMetadata.NextPassMetadata->AccessFlags);
					const bool layoutsDiffer       = (subresourceMetadata.Layout               != subresourceMetadata.NextPassMetadata->Layout);
					const bool queueFamiliesDiffer = (subresourceMetadata.QueueFamilyOwnership != subresourceMetadata.NextPassMetadata->QueueFamilyOwnership);
					if(accessFlagsDiffer || layoutsDiffer || queueFamiliesDiffer)
					{
						//Swapchain image barriers are changed every frame
						VkImage barrieredImage = nullptr;
						if(subresourceMetadata.ImageIndex != mGraphToBuild->mBackbufferRefIndex)
						{
							barrieredImage = mGraphToBuild->mImages[subresourceMetadata.ImageIndex];
						}

						VkImageMemoryBarrier imageMemoryBarrier;
						imageMemoryBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
						imageMemoryBarrier.pNext                           = nullptr;
						imageMemoryBarrier.srcAccessMask                   = subresourceMetadata.AccessFlags;
						imageMemoryBarrier.dstAccessMask                   = subresourceMetadata.NextPassMetadata->AccessFlags;
						imageMemoryBarrier.oldLayout                       = subresourceMetadata.Layout;
						imageMemoryBarrier.newLayout                       = subresourceMetadata.NextPassMetadata->Layout;
						imageMemoryBarrier.srcQueueFamilyIndex             = subresourceMetadata.QueueFamilyOwnership;
						imageMemoryBarrier.dstQueueFamilyIndex             = subresourceMetadata.NextPassMetadata->QueueFamilyOwnership;
						imageMemoryBarrier.image                           = mGraphToBuild->mImages[subresourceMetadata.ImageIndex];
						imageMemoryBarrier.subresourceRange.aspectMask     = subresourceMetadata.NextPassMetadata->AspectFlags;
						imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
						imageMemoryBarrier.subresourceRange.layerCount     = 1;
						imageMemoryBarrier.subresourceRange.baseMipLevel   = 0;
						imageMemoryBarrier.subresourceRange.levelCount     = 1;

						afterBarriers.push_back(imageMemoryBarrier);

						//Mark the swapchain barriers
						if(barrieredImage == nullptr)
						{
							afterSwapchainBarriers.push_back(afterBarriers.size() - 1);
						}
					}
				}
			}

			processedMetadataIds.insert(subresourceMetadata.MetadataId);
		}

		uint32_t beforeBarrierBegin = (uint32_t)(mGraphToBuild->mImageBarriers.size());
		mGraphToBuild->mImageBarriers.insert(mGraphToBuild->mImageBarriers.end(), beforeBarriers.begin(), beforeBarriers.end());

		uint32_t afterBarrierBegin = (uint32_t)(mGraphToBuild->mImageBarriers.size());
		mGraphToBuild->mImageBarriers.insert(mGraphToBuild->mImageBarriers.end(), afterBarriers.begin(), afterBarriers.end());

		FrameGraph::BarrierSpan barrierSpan;
		barrierSpan.BeforePassBegin = beforeBarrierBegin;
		barrierSpan.BeforePassEnd   = beforeBarrierBegin + (uint32_t)(beforeBarriers.size());
		barrierSpan.AfterPassBegin  = afterBarrierBegin;
		barrierSpan.AfterPassEnd    = afterBarrierBegin + (uint32_t)(afterBarriers.size());

		//That's for now
		barrierSpan.beforeFlagsBegin = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		barrierSpan.beforeFlagsEnd   = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		barrierSpan.afterFlagsBegin  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		barrierSpan.afterFlagsEnd    = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		mGraphToBuild->mImageRenderPassBarriers[i] = barrierSpan;

		for(size_t beforeSwapchainBarrierIndex: beforeSwapchainBarriers)
		{
			mGraphToBuild->mSwapchainBarrierIndices.push_back(beforeBarrierBegin + (uint32_t)beforeSwapchainBarrierIndex);
		}

		for(size_t afterSwapchainBarrierIndex: afterSwapchainBarriers)
		{
			mGraphToBuild->mSwapchainBarrierIndices.push_back(afterBarrierBegin + (uint32_t)afterSwapchainBarrierIndex);
		}
	}
}

void Vulkan::FrameGraphBuilder::BarrierImages(const DeviceQueues* deviceQueues, const WorkerCommandBuffers* workerCommandBuffers, uint32_t defaultQueueIndex)
{
	//Transit to the last pass as if rendering just ended. That ensures correct barriers at the start of the frame
	std::vector<VkImageMemoryBarrier> imageMemoryBarriers;

	std::unordered_map<SubresourceName, ImageSubresourceMetadata> imageLastMetadatas;
	for(size_t i = mRenderPassNames.size() - 1; i < mRenderPassNames.size(); i--) //The loop wraps around 0
	{
		const RenderPassName renderPassName = mRenderPassNames[i];

		const auto& nameIdMap = mRenderPassesSubresourceNameIds[renderPassName];
		for(const auto& nameId: nameIdMap)
		{
			if(!imageLastMetadatas.contains(nameId.first) && nameId.first != mBackbufferName)
			{
				ImageSubresourceMetadata metadata = mRenderPassesSubresourceMetadatas[renderPassName][nameId.second];
				imageLastMetadatas[nameId.first] = metadata;
			}
		}
	}

	std::vector<VkImageMemoryBarrier> imageBarriers;
	for(const auto& nameMetadata: imageLastMetadatas)
	{
		VkImageMemoryBarrier imageBarrier;
		imageBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageBarrier.pNext                           = nullptr;
		imageBarrier.srcAccessMask                   = 0;
		imageBarrier.dstAccessMask                   = nameMetadata.second.AccessFlags;
		imageBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
		imageBarrier.newLayout                       = nameMetadata.second.Layout;
		imageBarrier.srcQueueFamilyIndex             = defaultQueueIndex; //Resources were created with that queue ownership
		imageBarrier.dstQueueFamilyIndex             = nameMetadata.second.QueueFamilyOwnership;
		imageBarrier.image                           = mGraphToBuild->mImages[nameMetadata.second.ImageIndex];
		imageBarrier.subresourceRange.aspectMask     = nameMetadata.second.AspectFlags;
		imageBarrier.subresourceRange.baseMipLevel   = 0;
		imageBarrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
		imageBarrier.subresourceRange.baseArrayLayer = 0;
		imageBarrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

		imageBarriers.push_back(imageBarrier);
	}

	VkCommandPool   graphicsCommandPool   = workerCommandBuffers->GetMainThreadGraphicsCommandPool(0);
	VkCommandBuffer graphicsCommandBuffer = workerCommandBuffers->GetMainThreadGraphicsCommandBuffer(0);

	ThrowIfFailed(vkResetCommandPool(mGraphToBuild->mDeviceRef, graphicsCommandPool, 0));

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
		                 (uint32_t)memoryBarriers.size(), memoryBarriers.data(),
		                 (uint32_t)bufferBarriers.size(), bufferBarriers.data(),
		                 (uint32_t)imageBarriers.size(),  imageBarriers.data());

	ThrowIfFailed(vkEndCommandBuffer(graphicsCommandBuffer));

	deviceQueues->GraphicsQueueSubmit(graphicsCommandBuffer);
	deviceQueues->GraphicsQueueWait();
}

void Vulkan::FrameGraphBuilder::CreateTextures(const std::vector<TextureResourceCreateInfo>& textureCreateInfos) const
{
	mVulkanGraphToBuild->mImages.resize(textureCreateInfos.size());

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

		VkImage image = VK_NULL_HANDLE;
		ThrowIfFailed(vkCreateImage(mVulkanGraphToBuild->mDeviceRef, &imageCreateInfo, nullptr, &image));

		SetDebugObjectName(image, textureCreateInfo.Name); //Only does something in debug

		mVulkanGraphToBuild->mImages[headNode->ImageIndex] = image;
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
}

void Vulkan::FrameGraphBuilder::AllocateTextureViews(size_t textureViewCount)
{
	mVulkanGraphToBuild->mImageViews.resize(textureViewCount);
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

bool Vulkan::FrameGraphBuilder::PropagateSubresourceParameters(uint32_t indexFrom, uint32_t indexTo)
{
	SubresourceInfo& currentPassSubresourceInfo = mSubresourceInfos[indexFrom];
	SubresourceInfo& nextPassSubresourceInfo    = mSubresourceInfos[indexTo];

	if(nextPassSubresourceInfo.Format == VK_FORMAT_UNDEFINED && currentPassSubresourceInfo.Format != VK_FORMAT_UNDEFINED)
	{
		nextPassSubresourceInfo.Format = currentPassSubresourceInfo.Format;
	}

	if(nextPassSubresourceInfo.Aspect == 0 && currentPassSubresourceInfo.Aspect != 0)
	{
		nextPassSubresourceInfo.Aspect = currentPassSubresourceInfo.Aspect;
	}

	//Other parameters are always fully known

	if(currentPassSubresourceInfo.Format == VK_FORMAT_UNDEFINED || currentPassSubresourceInfo.Aspect == 0)
	{
		return false;
	}

	return true;
}

void Vulkan::FrameGraphBuilder::BuildUniqueSubresourceList(const std::vector<uint32_t>& subresourceInfoIndices, std::vector<uint32_t>& outUniqueSubresourceInfoIndices, std::vector<uint32_t>& outNewIndexMap)
{
	if(subresourceInfoIndices.size() == 0)
	{
		return;
	}

	std::vector<uint32_t> reorderMap(subresourceInfoIndices.size());
	std::iota(reorderMap.begin(), reorderMap.end(), 0);

	std::sort(reorderMap.begin(), reorderMap.end(), [this, &subresourceInfoIndices](uint32_t left, uint32_t right)
	{
		const uint32_t leftIndex  = subresourceInfoIndices[left];
		const uint32_t rightIndex = subresourceInfoIndices[right];

		const SubresourceInfo& leftSubresource  = mSubresourceInfos[leftIndex];
		const SubresourceInfo& rightSubresource = mSubresourceInfos[rightIndex];

		if(leftSubresource.Format == rightSubresource.Format)
		{
			return (uint32_t)leftSubresource.Aspect < (uint32_t)rightSubresource.Aspect;
		}
		else
		{
			return (uint32_t)leftSubresource.Format < (uint32_t)rightSubresource.Format;
		}
	});

	outUniqueSubresourceInfoIndices.clear();
	outUniqueSubresourceInfoIndices.push_back(subresourceInfoIndices[reorderMap[0]]);

	outNewIndexMap.resize(subresourceInfoIndices.size());
	outNewIndexMap[reorderMap[0]] = 0;

	for(size_t i = 1; i < reorderMap.size(); i++)
	{
		uint32_t leftIndicesIndex  = reorderMap[i - 1];
		uint32_t rightIndicesIndex = reorderMap[i];

		uint32_t leftIndex  = subresourceInfoIndices[leftIndicesIndex];
		uint32_t rightIndex = subresourceInfoIndices[rightIndicesIndex];

		const SubresourceInfo& leftSubresource  = mSubresourceInfos[leftIndex];
		const SubresourceInfo& rightSubresource = mSubresourceInfos[rightIndex];

		if(leftSubresource.Format != rightSubresource.Format || leftSubresource.Aspect != rightSubresource.Aspect)
		{
			outUniqueSubresourceInfoIndices.push_back(subresourceInfoIndices[rightIndicesIndex]);
		}

		outNewIndexMap[reorderMap[i]] = outUniqueSubresourceInfoIndices.size() - 1;
	}
}

void Vulkan::FrameGraphBuilder::CreateTextureViews(const std::vector<TextureResourceCreateInfo>& textureCreateInfos, const std::vector<uint32_t>& subresourceInfoIndices) const
{
	for(const TextureResourceCreateInfo& textureCreateInfo: textureCreateInfos)
	{
		TextureSubresourceInfoSpan subresourceInfoSpan = textureCreateInfo.SubresourceSpan;

		uint32_t imageIndex = textureCreateInfo.MetadataHead->ImageIndex;
		VkImage  image      = mVulkanGraphToBuild->mImages[imageIndex];
		for(uint32_t subresourceInfoIndexIndex = subresourceInfoSpan.FirstSubresourceInfoIndex; subresourceInfoIndexIndex < subresourceInfoSpan.LastSubresourceInfoIndex; subresourceInfoIndexIndex++)
		{
			uint32_t subresourceInfoIndex = subresourceInfoIndices[subresourceInfoIndexIndex];

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

			mVulkanGraphToBuild->mImageViews[subresourceInfoIndex] = imageView;
		}
	}
}

uint32_t Vulkan::FrameGraphBuilder::AllocateBackbufferResources() const
{
	mVulkanGraphToBuild->mImages.push_back(VK_NULL_HANDLE);

	mVulkanGraphToBuild->mSwapchainImageRefs.clear();
	for(size_t i = 0; i < SwapChain::SwapchainImageCount; i++)
	{
		mVulkanGraphToBuild->mSwapchainImageRefs.push_back(mSwapChain->GetSwapchainImage(i));
	}

	mVulkanGraphToBuild->mSwapchainImageViews.clear();
	mVulkanGraphToBuild->mSwapchainViewsSwapMap.clear();

	return (uint32_t)(mVulkanGraphToBuild->mImages.size() - 1);
}

void Vulkan::FrameGraphBuilder::SetDebugObjectName(VkImage image, const std::string_view name) const
{
#if defined(VK_EXT_debug_utils) && (defined(DEBUG) || defined(_DEBUG))
	
	if(mInstanceParameters->IsDebugUtilsExtensionEnabled())
	{
		VkDebugUtilsObjectNameInfoEXT imageNameInfo;
		imageNameInfo.sType        = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		imageNameInfo.pNext        = nullptr;
		imageNameInfo.objectType   = VK_OBJECT_TYPE_IMAGE;
		imageNameInfo.objectHandle = reinterpret_cast<uint64_t>(image);
		imageNameInfo.pObjectName  = name.data();

		ThrowIfFailed(vkSetDebugUtilsObjectNameEXT(mVulkanGraphToBuild->mDeviceRef, &imageNameInfo));
	}
#else
	UNREFERENCED_PARAMETER(image);
	UNREFERENCED_PARAMETER(name);
#endif
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
