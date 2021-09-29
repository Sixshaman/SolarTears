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

Vulkan::FrameGraphBuilder::FrameGraphBuilder(LoggerQueue* logger, FrameGraph* graphToBuild, const SwapChain* swapchain): ModernFrameGraphBuilder(graphToBuild), mLogger(logger), mVulkanGraphToBuild(graphToBuild), mSwapChain(swapchain)
{
	mShaderDatabase = std::make_unique<ShaderDatabase>(mLogger);
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

VkImage Vulkan::FrameGraphBuilder::GetRegisteredResource(uint32_t passIndex, uint_fast16_t subresourceIndex, uint32_t frame) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;

	const SubresourceMetadataNode& metadataNode = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex];
	const ResourceMetadata& resourceMetadata = mResourceMetadatas[metadataNode.ResourceMetadataIndex];
	if(resourceMetadata.FirstFrameHandle == (uint32_t)(-1))
	{
		return VK_NULL_HANDLE;
	}
	else
	{
		if(resourceMetadata.FirstFrameHandle == GetBackbufferImageSpan().Begin)
		{
			uint32_t passPeriod = mPassMetadatas[passIndex].OwnPeriod;
			return mVulkanGraphToBuild->mImages[resourceMetadata.FirstFrameHandle + frame / passPeriod];
		}
		else
		{
			
			return mVulkanGraphToBuild->mImages[resourceMetadata.FirstFrameHandle + frame % resourceMetadata.FrameCount];
		}
	}
}

VkImageView Vulkan::FrameGraphBuilder::GetRegisteredSubresource(uint32_t passIndex, uint_fast16_t subresourceIndex, uint32_t frame) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;

	const SubresourceMetadataNode& metadataNode = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex];
	const ResourceMetadata& resourceMetadata = mResourceMetadatas[metadataNode.ResourceMetadataIndex];
	if(metadataNode.FirstFrameViewHandle == (uint32_t)(-1))
	{
		return VK_NULL_HANDLE;
	}
	else
	{
		if(resourceMetadata.FirstFrameHandle == GetBackbufferImageSpan().Begin)
		{
			uint32_t passPeriod = mPassMetadatas[passIndex].OwnPeriod;
			return mVulkanGraphToBuild->mImageViews[metadataNode.FirstFrameViewHandle + frame / passPeriod];
		}
		else
		{
			
			return mVulkanGraphToBuild->mImageViews[metadataNode.FirstFrameViewHandle + frame % resourceMetadata.FrameCount];
		}
	}
}

VkFormat Vulkan::FrameGraphBuilder::GetRegisteredSubresourceFormat(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;
	return mSubresourceMetadataPayloads[metadataSpan.Begin + subresourceIndex].Format;
}

VkImageLayout Vulkan::FrameGraphBuilder::GetRegisteredSubresourceLayout(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;
	return mSubresourceMetadataPayloads[metadataSpan.Begin + subresourceIndex].Layout;
}

VkImageUsageFlags Vulkan::FrameGraphBuilder::GetRegisteredSubresourceUsage(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;
	return mSubresourceMetadataPayloads[metadataSpan.Begin + subresourceIndex].Usage;
}

VkPipelineStageFlags Vulkan::FrameGraphBuilder::GetRegisteredSubresourceStageFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;
	return mSubresourceMetadataPayloads[metadataSpan.Begin + subresourceIndex].Stage;
}

VkImageAspectFlags Vulkan::FrameGraphBuilder::GetRegisteredSubresourceAspectFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;
	return mSubresourceMetadataPayloads[metadataSpan.Begin + subresourceIndex].Aspect;
}

VkAccessFlags Vulkan::FrameGraphBuilder::GetRegisteredSubresourceAccessFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;
	return mSubresourceMetadataPayloads[metadataSpan.Begin + subresourceIndex].Access;
}

VkImageLayout Vulkan::FrameGraphBuilder::GetPreviousPassSubresourceLayout(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;
	uint32_t prevNodeIndex = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex].PrevPassNodeIndex;
	return mSubresourceMetadataPayloads[prevNodeIndex].Layout;
}

VkImageUsageFlags Vulkan::FrameGraphBuilder::GetPreviousPassSubresourceUsage(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;
	uint32_t prevNodeIndex = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex].PrevPassNodeIndex;
	return mSubresourceMetadataPayloads[prevNodeIndex].Usage;
}

VkPipelineStageFlags Vulkan::FrameGraphBuilder::GetPreviousPassSubresourceStageFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;
	uint32_t prevNodeIndex = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex].PrevPassNodeIndex;
	return mSubresourceMetadataPayloads[prevNodeIndex].Stage;
}

VkImageAspectFlags Vulkan::FrameGraphBuilder::GetPreviousPassSubresourceAspectFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;
	uint32_t prevNodeIndex = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex].PrevPassNodeIndex;
	return mSubresourceMetadataPayloads[prevNodeIndex].Aspect;
}

VkAccessFlags Vulkan::FrameGraphBuilder::GetPreviousPassSubresourceAccessFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;
	uint32_t prevNodeIndex = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex].PrevPassNodeIndex;
	return mSubresourceMetadataPayloads[prevNodeIndex].Access;
}

VkImageLayout Vulkan::FrameGraphBuilder::GetNextPassSubresourceLayout(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;
	uint32_t nextNodeIndex = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex].NextPassNodeIndex;
	return mSubresourceMetadataPayloads[nextNodeIndex].Layout;
}

VkImageUsageFlags Vulkan::FrameGraphBuilder::GetNextPassSubresourceUsage(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;
	uint32_t nextNodeIndex = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex].NextPassNodeIndex;
	return mSubresourceMetadataPayloads[nextNodeIndex].Usage;
}

VkPipelineStageFlags Vulkan::FrameGraphBuilder::GetNextPassSubresourceStageFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;
	uint32_t nextNodeIndex = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex].NextPassNodeIndex;
	return mSubresourceMetadataPayloads[nextNodeIndex].Stage;
}

VkImageAspectFlags Vulkan::FrameGraphBuilder::GetNextPassSubresourceAspectFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;
	uint32_t nextNodeIndex = mSubresourceMetadataNodesFlat[metadataSpan.Begin + subresourceIndex].NextPassNodeIndex;
	return mSubresourceMetadataPayloads[nextNodeIndex].Aspect;
}

VkAccessFlags Vulkan::FrameGraphBuilder::GetNextPassSubresourceAccessFlags(uint32_t passIndex, uint_fast16_t subresourceIndex) const
{
	Span<uint32_t> metadataSpan = mPassMetadatas[passIndex].SubresourceMetadataSpan;
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

void Vulkan::FrameGraphBuilder::BuildPassDescriptorSets()
{
	//Step 1 - find unique pass set infos
	std::vector<PassSetInfo> uniquePassSetInfos;
	std::vector<uint16_t> passSetBindingTypes;
	mShaderDatabase->BuildUniquePassSetInfos(uniquePassSetInfos, passSetBindingTypes);


	//Step 2 - build flat pass set info list corresponding to the frame graph passes
	struct NonUniquePassSetInfo
	{
		uint32_t       PassIndex;
		Span<uint32_t> BindingIndexSpan;
		uint32_t       SetCount;
	};

	std::sort(uniquePassSetInfos.begin(), uniquePassSetInfos.end(), [](const PassSetInfo& left, const PassSetInfo& right)
	{
		return left.PassType < right.PassType;
	});

	std::vector<NonUniquePassSetInfo>  nonUniqueSetInfos;
	std::vector<VkDescriptorSetLayout> setLayoutsFlat;
	for(uint32_t passIndex = 0; passIndex < mPassMetadatas.size(); passIndex++)
	{
		auto passSetInfoRange = std::equal_range(uniquePassSetInfos.begin(), uniquePassSetInfos.end(), mPassMetadatas[passIndex].Type, [](const PassSetInfo& passSetInfo, RenderPassType type)
		{
			return passSetInfo.PassType < type;
		});

		std::span setInfoSpan = {passSetInfoRange.first, passSetInfoRange.second};
		for(const PassSetInfo& passSetInfo: setInfoSpan)
		{
			uint32_t setCount = 1;
			for(uint32_t bindingIndex = passSetInfo.BindingSpan.Begin; bindingIndex < passSetInfo.BindingSpan.End; bindingIndex++)
			{
				uint32_t subresourceIndex = mPassMetadatas[passIndex].SubresourceMetadataSpan.Begin + passSetBindingTypes[bindingIndex];

				const SubresourceMetadataNode& metadataNode     = mSubresourceMetadataNodesFlat[subresourceIndex];
				const ResourceMetadata&        resourceMetadata = mResourceMetadatas[metadataNode.ResourceMetadataIndex];

				setCount = std::lcm(mResourceMetadatas[metadataNode.ResourceMetadataIndex].FrameCount, setCount);
			}

			nonUniqueSetInfos.push_back(NonUniquePassSetInfo
			{
				.PassIndex        = passIndex,
				.BindingIndexSpan = passSetInfo.BindingSpan,
				.SetCount         = setCount
			});

			setLayoutsFlat.resize(setLayoutsFlat.size() + setCount, passSetInfo.SetLayout);
		}
	}


	//Step 3 - find the pool sizes
	VkDescriptorPoolSize sampledImagePoolSize    = {.type = VK_DESCRIPTOR_TYPE_SAMPLER,        .descriptorCount = 0};
	VkDescriptorPoolSize storageImagePoolSize    = {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  .descriptorCount = 0};
	VkDescriptorPoolSize inputAttachmentPoolSize = {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 0};

	for(const NonUniquePassSetInfo& passSetInfo: nonUniqueSetInfos)
	{
		const PassMetadata& passMetadata = mPassMetadatas[passSetInfo.PassIndex];
		for(uint32_t bindingIndex = passSetInfo.BindingIndexSpan.Begin; bindingIndex < passSetInfo.BindingIndexSpan.End; bindingIndex++)
		{
			uint32_t subresourceIndex = passMetadata.SubresourceMetadataSpan.Begin + passSetBindingTypes[bindingIndex];
			const SubresourceMetadataPayload& metadataPayload = mSubresourceMetadataPayloads[subresourceIndex];

			if(metadataPayload.Usage == VK_IMAGE_USAGE_SAMPLED_BIT)
			{
				sampledImagePoolSize.descriptorCount += passSetInfo.SetCount;
			}
			else if(metadataPayload.Usage == VK_IMAGE_USAGE_STORAGE_BIT)
			{
				storageImagePoolSize.descriptorCount += passSetInfo.SetCount;
			}
			else if(metadataPayload.Usage == VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
			{
				inputAttachmentPoolSize.descriptorCount += passSetInfo.SetCount;
			}
		}
	}


	//Step 4
	std::array poolSizes = {sampledImagePoolSize, storageImagePoolSize, storageImagePoolSize};

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
	descriptorPoolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext         = nullptr;
	descriptorPoolCreateInfo.flags         = 0;
	descriptorPoolCreateInfo.maxSets       = (uint32_t)setLayoutsFlat.size();
	descriptorPoolCreateInfo.poolSizeCount = (uint32_t)(poolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes    = poolSizes.data();

	ThrowIfFailed(vkCreateDescriptorPool(mVulkanGraphToBuild->mDeviceRef, &descriptorPoolCreateInfo, nullptr, &mDescriptorPool));


	//Step 5
	VkDescriptorSetAllocateInfo setAllocateInfo;
	setAllocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocateInfo.pNext              = nullptr;
	setAllocateInfo.descriptorPool     = mDescriptorPool;
	setAllocateInfo.descriptorSetCount = (uint32_t)setLayoutsFlat.size();
	setAllocateInfo.pSetLayouts        = setLayoutsFlat.data();

	ThrowIfFailed(vkAllocateDescriptorSets(mVulkanGraphToBuild->mDeviceRef, &setAllocateInfo, mSets.data()));


	//Step 6
	std::vector<VkWriteDescriptorSet> writeDescriptorSets;
	std::vector<VkDescriptorImageInfo> descriptorImageInfos;
	for(uint32_t passInfoIndex = 0, totalSetIndex = 0; passInfoIndex < nonUniqueSetInfos.size(); passInfoIndex++)
	{
		const NonUniquePassSetInfo& passSetInfo = nonUniqueSetInfos[passInfoIndex];

		const PassMetadata& passMetadata = mPassMetadatas[passSetInfo.PassIndex];
		for(uint32_t setIndex = 0; setIndex < passSetInfo.SetCount; setIndex++, totalSetIndex++)
		{
			for(uint32_t bindingIndex = passSetInfo.BindingIndexSpan.Begin; bindingIndex < passSetInfo.BindingIndexSpan.End; bindingIndex++)
			{
				uint32_t subresourceIndex = passMetadata.SubresourceMetadataSpan.Begin + passSetBindingTypes[bindingIndex];

				const SubresourceMetadataPayload& metadataPayload  = mSubresourceMetadataPayloads[subresourceIndex];
				const SubresourceMetadataNode&    metadataNode     = mSubresourceMetadataNodesFlat[subresourceIndex];
				const ResourceMetadata&           resourceMetadata = mResourceMetadatas[metadataNode.ResourceMetadataIndex];

				VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
				if(metadataPayload.Usage == VK_IMAGE_USAGE_SAMPLED_BIT)
				{
					type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
				}
				else if(metadataPayload.Usage == VK_IMAGE_USAGE_STORAGE_BIT)
				{
					type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				}
				else if(metadataPayload.Usage == VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
				{
					type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
				}

				uint32_t layoutBindingIndex = bindingIndex - passSetInfo.BindingIndexSpan.Begin;

				VkDescriptorImageInfo descriptorImageInfo;
				descriptorImageInfo.sampler     = nullptr;
				descriptorImageInfo.imageView   = mVulkanGraphToBuild->mImageViews[metadataNode.FirstFrameViewHandle + setIndex % resourceMetadata.FrameCount];
				descriptorImageInfo.imageLayout = metadataPayload.Layout;
				descriptorImageInfos.push_back(descriptorImageInfo);

				VkWriteDescriptorSet writeDescriptorSet;
				writeDescriptorSet.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSet.pNext            = nullptr;
				writeDescriptorSet.dstSet           = mSets[totalSetIndex];
				writeDescriptorSet.dstBinding       = layoutBindingIndex;
				writeDescriptorSet.dstArrayElement  = 0;
				writeDescriptorSet.descriptorCount  = 1;
				writeDescriptorSet.descriptorType   = type;
				writeDescriptorSet.pImageInfo       = &descriptorImageInfos.back();
				writeDescriptorSet.pBufferInfo      = nullptr;
				writeDescriptorSet.pTexelBufferView = nullptr;

				writeDescriptorSets.push_back(writeDescriptorSet);
			}
		}
	}

	vkUpdateDescriptorSets(mVulkanGraphToBuild->mDeviceRef, (uint32_t)writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
}

void Vulkan::FrameGraphBuilder::InitMetadataPayloads()
{
	mSubresourceMetadataPayloads.resize(mSubresourceMetadataNodesFlat.size(), SubresourceMetadataPayload());

	for(const PassMetadata& passMetadata: mPassMetadatas)
	{
		Span<uint32_t> passMetadataSpan = passMetadata.SubresourceMetadataSpan;
		std::span<SubresourceMetadataPayload> payloadSpan = {mSubresourceMetadataPayloads.begin() + passMetadataSpan.Begin, mSubresourceMetadataPayloads.begin() + passMetadataSpan.End};

		RegisterPassSubresources(passMetadata.Type, payloadSpan);

		mShaderDatabase->RegisterPass(passMetadata.Type);
	}

	uint32_t backbufferPayloadIndex = mPresentPassMetadata.SubresourceMetadataSpan.Begin + (uint32_t)PresentPassSubresourceId::Backbuffer;
	mSubresourceMetadataPayloads[backbufferPayloadIndex].Format = mSwapChain->GetBackbufferFormat();
	mSubresourceMetadataPayloads[backbufferPayloadIndex].Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	mSubresourceMetadataPayloads[backbufferPayloadIndex].Layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	mSubresourceMetadataPayloads[backbufferPayloadIndex].Usage  = 0;
	mSubresourceMetadataPayloads[backbufferPayloadIndex].Stage  = VK_PIPELINE_STAGE_NONE_KHR;
	mSubresourceMetadataPayloads[backbufferPayloadIndex].Access = 0;
	mSubresourceMetadataPayloads[backbufferPayloadIndex].Flags  = 0;
}

bool Vulkan::FrameGraphBuilder::IsReadSubresource(uint32_t subresourceInfoIndex)
{
	return mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_GENERAL
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
}

bool Vulkan::FrameGraphBuilder::IsWriteSubresource(uint32_t subresourceInfoIndex)
{
	return mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL
		|| mSubresourceMetadataPayloads[subresourceInfoIndex].Layout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
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

		//Aspect gets propagated from the previous pass to the current pass
		if(nextSubresourcePayload.Aspect == 0 && currSubresourcePayload.Aspect != 0)
		{
			nextSubresourcePayload.Aspect = currSubresourcePayload.Aspect;
			propagationHappened = true;
		}

		//Format gets propagated from the previous pass to the current pass
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
	mVulkanGraphToBuild->mImages.clear();
	uint32_t backbufferResourceIndex = mSubresourceMetadataNodesFlat[mPresentPassMetadata.SubresourceMetadataSpan.Begin + (size_t)PresentPassSubresourceId::Backbuffer].ResourceMetadataIndex;

	std::vector<VkImageMemoryBarrier> imageInitialStateBarriers; //Barriers to initialize images
	for(uint32_t resourceMetadataIndex = 0; resourceMetadataIndex < mResourceMetadatas.size(); resourceMetadataIndex++)
	{
		if(resourceMetadataIndex == backbufferResourceIndex)
		{
			continue; //Backbuffer is handled separately
		}

		ResourceMetadata& textureMetadata = mResourceMetadatas[resourceMetadataIndex];
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
			const SubresourceMetadataNode&    subresourceMetadata        = mSubresourceMetadataNodesFlat[currentNodeIndex];
			const SubresourceMetadataPayload& subresourceMetadataPayload = mSubresourceMetadataPayloads[currentNodeIndex];

			imageQueueIndices[0] = PassClassToQueueIndex(subresourceMetadata.PassClass);
			imageCreateInfo.usage |= subresourceMetadataPayload.Usage;
			
			imageInitialBarrier.dstAccessMask               = subresourceMetadataPayload.Access;
			imageInitialBarrier.dstQueueFamilyIndex         = PassClassToQueueIndex(subresourceMetadata.PassClass);
			imageInitialBarrier.subresourceRange.aspectMask = subresourceMetadataPayload.Aspect;

			//TODO: mutable format lists
			assert(subresourceMetadataPayload.Format != VK_FORMAT_UNDEFINED);
			if(imageCreateInfo.format != subresourceMetadataPayload.Format)
			{
				assert(formatChanges < 255);
				formatChanges++;

				imageCreateInfo.format = subresourceMetadataPayload.Format;
			}

			if(subresourceMetadataPayload.Layout != VK_FORMAT_UNDEFINED)
			{
				imageInitialBarrier.newLayout = subresourceMetadataPayload.Layout;
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
					VulkanUtils::SetDebugObjectName(mVulkanGraphToBuild->mDeviceRef, image, textureMetadata.Name + std::to_string(frame));
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
	ResourceMetadata& backbufferMetadata = mResourceMetadatas[backbufferResourceIndex];

	uint32_t backbufferImageIndex = (uint32_t)mVulkanGraphToBuild->mImages.size();
	backbufferMetadata.FirstFrameHandle = backbufferImageIndex;
	for(uint32_t frame = 0; frame < backbufferMetadata.FrameCount; frame++)
	{
		VkImage swapchainImage = mSwapChain->GetSwapchainImage(frame);

#if defined(DEBUG) || defined(_DEBUG)
		VulkanUtils::SetDebugObjectName(mVulkanGraphToBuild->mDeviceRef, swapchainImage, backbufferMetadata.Name + std::to_string(frame));
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

void Vulkan::FrameGraphBuilder::CreateTextureViews()
{
	mVulkanGraphToBuild->mImageViews.clear();
	for(const ResourceMetadata& resourceMetadata: mResourceMetadatas)
	{
		std::unordered_map<uint64_t, uint32_t> imageViewIndicesForViewInfos; //To only create different image views if the format + aspect flags differ

		uint32_t headNodeIndex = resourceMetadata.HeadNodeIndex;
		uint32_t currNodeIndex = headNodeIndex;
		do
		{
			SubresourceMetadataNode&          subresourceMetadata        = mSubresourceMetadataNodesFlat[currNodeIndex];
			const SubresourceMetadataPayload& subresourceMetadataPayload = mSubresourceMetadataPayloads[currNodeIndex];

			if(subresourceMetadataPayload.Format == VK_FORMAT_UNDEFINED || subresourceMetadataPayload.Aspect == 0)
			{
				//The resource doesn't require a view in the pass
				continue;
			}

			uint64_t viewInfoKey = (subresourceMetadataPayload.Aspect << 32) | (subresourceMetadataPayload.Format);
			
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
					mVulkanGraphToBuild->mImageViews[newImageViewIndex + frameIndex] = CreateImageView(image, subresourceMetadataPayload.Format, subresourceMetadataPayload.Aspect);
				}

				subresourceMetadata.FirstFrameViewHandle  = newImageViewIndex;
				imageViewIndicesForViewInfos[viewInfoKey] = newImageViewIndex;
			}
			
			currNodeIndex = subresourceMetadata.NextPassNodeIndex;

		} while(currNodeIndex != headNodeIndex);
	}
}

void Vulkan::FrameGraphBuilder::CreateObjectsForPass(uint32_t passMetadataIndex, uint32_t passSwapchainImageCount)
{
	uint32_t oldPassCount = (uint32_t)mVulkanGraphToBuild->mRenderPasses.size();

	const PassMetadata& passMetadata = mPassMetadatas[passMetadataIndex];
	for(uint32_t swapchainImageIndex = 0; swapchainImageIndex < passSwapchainImageCount; swapchainImageIndex++)
	{
		for(uint32_t passFrame = 0; passFrame < passMetadata.OwnPeriod; passFrame++)
		{
			uint32_t frame = passMetadata.OwnPeriod * swapchainImageIndex + passFrame;
			
			mVulkanGraphToBuild->mRenderPasses.emplace_back(MakeUniquePass(passMetadata.Type, this, passMetadataIndex, frame));
		}
	}

	ModernFrameGraph::RenderPassSpanInfo passSpanInfo;
	passSpanInfo.PassSpanBegin = oldPassCount;
	passSpanInfo.OwnFrames     = passMetadata.OwnPeriod;

	mVulkanGraphToBuild->mPassFrameSpans.push_back(passSpanInfo);
}

void Vulkan::FrameGraphBuilder::AddBeforePassBarriers(const PassMetadata& passMetadata, uint32_t barrierSpanIndex)
{
	mVulkanGraphToBuild->mRenderPassBarriers[barrierSpanIndex].BeforePassBegin = (uint32_t)mVulkanGraphToBuild->mImageBarriers.size();

	for(uint32_t metadataIndex = passMetadata.SubresourceMetadataSpan.Begin; metadataIndex < passMetadata.SubresourceMetadataSpan.End; metadataIndex++)
	{
		const SubresourceMetadataNode& currMetadataNode = mSubresourceMetadataNodesFlat[metadataIndex];
		const SubresourceMetadataNode& prevMetadataNode = mSubresourceMetadataNodesFlat[currMetadataNode.PrevPassNodeIndex];

		const SubresourceMetadataPayload& currMetadataPayload = mSubresourceMetadataPayloads[metadataIndex];
		const SubresourceMetadataPayload& prevMetadataPayload = mSubresourceMetadataPayloads[currMetadataNode.PrevPassNodeIndex];

		const ResourceMetadata& resourceMetadata = mResourceMetadatas[currMetadataNode.ResourceMetadataIndex];

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
				imageMemoryBarrier.image                           = mVulkanGraphToBuild->mImages[resourceMetadata.FirstFrameHandle];
				imageMemoryBarrier.subresourceRange.aspectMask     = prevMetadataPayload.Aspect | currMetadataPayload.Aspect;
				imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
				imageMemoryBarrier.subresourceRange.layerCount     = 1;
				imageMemoryBarrier.subresourceRange.baseMipLevel   = 0;
				imageMemoryBarrier.subresourceRange.levelCount     = 1;

				mVulkanGraphToBuild->mImageBarriers.push_back(imageMemoryBarrier);

				if(resourceMetadata.FrameCount > 1)
				{
					uint32_t newBarrierIndex = (uint32_t)(mVulkanGraphToBuild->mImageBarriers.size() - 1);

					ModernFrameGraph::MultiframeBarrierInfo multiframeBarrierInfo;
					multiframeBarrierInfo.BarrierIndex  = newBarrierIndex;
					multiframeBarrierInfo.BaseTexIndex  = resourceMetadata.FirstFrameHandle;
					multiframeBarrierInfo.TexturePeriod = resourceMetadata.FrameCount;

					mVulkanGraphToBuild->mMultiframeBarrierInfos.push_back(multiframeBarrierInfo);
				}
			}
		}
	}

	mVulkanGraphToBuild->mRenderPassBarriers[barrierSpanIndex].BeforePassEnd = (uint32_t)mVulkanGraphToBuild->mImageBarriers.size();
}

void Vulkan::FrameGraphBuilder::AddAfterPassBarriers(const PassMetadata& passMetadata, uint32_t barrierSpanIndex)
{
	mVulkanGraphToBuild->mRenderPassBarriers[barrierSpanIndex].AfterPassBegin = (uint32_t)mVulkanGraphToBuild->mImageBarriers.size();

	for(uint32_t metadataIndex = passMetadata.SubresourceMetadataSpan.Begin; metadataIndex < passMetadata.SubresourceMetadataSpan.End; metadataIndex++)
	{
		const SubresourceMetadataNode& currMetadataNode = mSubresourceMetadataNodesFlat[metadataIndex];
		const SubresourceMetadataNode& nextMetadataNode = mSubresourceMetadataNodesFlat[currMetadataNode.NextPassNodeIndex];

		const SubresourceMetadataPayload& currMetadataPayload = mSubresourceMetadataPayloads[metadataIndex];
		const SubresourceMetadataPayload& nextMetadataPayload = mSubresourceMetadataPayloads[currMetadataNode.NextPassNodeIndex];

		const ResourceMetadata& resourceMetadata = mResourceMetadatas[currMetadataNode.ResourceMetadataIndex];

		if(!(currMetadataPayload.Flags & TextureFlagAutoAfterBarrier) && !(nextMetadataPayload.Flags & TextureFlagAutoBeforeBarrier))
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
				imageMemoryBarrier.image                           = mVulkanGraphToBuild->mImages[resourceMetadata.FirstFrameHandle];
				imageMemoryBarrier.subresourceRange.aspectMask     = currMetadataPayload.Aspect | nextMetadataPayload.Aspect;
				imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
				imageMemoryBarrier.subresourceRange.layerCount     = 1;
				imageMemoryBarrier.subresourceRange.baseMipLevel   = 0;
				imageMemoryBarrier.subresourceRange.levelCount     = 1;

				mVulkanGraphToBuild->mImageBarriers.push_back(imageMemoryBarrier);

				if(resourceMetadata.FrameCount > 1)
				{
					uint32_t newBarrierIndex = (uint32_t)(mVulkanGraphToBuild->mImageBarriers.size() - 1);

					ModernFrameGraph::MultiframeBarrierInfo multiframeBarrierInfo;
					multiframeBarrierInfo.BarrierIndex  = newBarrierIndex;
					multiframeBarrierInfo.BaseTexIndex  = resourceMetadata.FirstFrameHandle;
					multiframeBarrierInfo.TexturePeriod = resourceMetadata.FrameCount;

					mVulkanGraphToBuild->mMultiframeBarrierInfos.push_back(multiframeBarrierInfo);
				}
			}
		}
	}

	mVulkanGraphToBuild->mRenderPassBarriers[barrierSpanIndex].AfterPassEnd = (uint32_t)mVulkanGraphToBuild->mImageBarriers.size();
}

void Vulkan::FrameGraphBuilder::InitializeTraverseData() const
{
	mVulkanGraphToBuild->mFrameRecordedGraphicsCommandBuffers.resize(mVulkanGraphToBuild->mGraphicsPassSpans.size());

	//Add a plug present pass
	mVulkanGraphToBuild->mPassFrameSpans.push_back(ModernFrameGraph::RenderPassSpanInfo
	{
		.PassSpanBegin = (uint32_t)mVulkanGraphToBuild->mRenderPasses.size(),
		.OwnFrames     = 0
	});
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
