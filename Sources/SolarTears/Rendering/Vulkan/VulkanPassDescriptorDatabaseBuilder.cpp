#include "VulkanPassDescriptorDatabaseBuilder.hpp"
#include "VulkanDescriptors.hpp"
#include "VulkanUtils.hpp"
#include "VulkanFunctions.hpp"
#include "VulkanShaders.hpp"
#include "FrameGraph/VulkanFrameGraphBuilder.hpp"
#include <span>
#include <algorithm>

Vulkan::PassDescriptorDatabaseBuilder::PassDescriptorDatabaseBuilder(DescriptorDatabase* databaseToBuild): mDatabaseToBuild(databaseToBuild)
{
}

Vulkan::PassDescriptorDatabaseBuilder::~PassDescriptorDatabaseBuilder()
{
}

void Vulkan::PassDescriptorDatabaseBuilder::AddPassSetInfo(VkDescriptorSetLayout setLayout, uint32_t passIndex, std::span<const uint16_t> setBindingTypes)
{
	mPassSetLayouts.push_back(setLayout);
	mPassSetInfosPerLayout.push_back(PassSetInfo
	{
		.PassIndex               = passIndex,
		.DatabaseDescriptorIndex = (uint32_t)(mDatabaseToBuild->mDescriptorSets.size()),
		.BindingIndexSpan        = Span<uint32_t>
		{
			.Begin = (uint32_t)(mPassSetBindingTypes.size()),
			.End   = (uint32_t)(mPassSetBindingTypes.size() + setBindingTypes.size())
		}
	});

	mPassSetBindingTypes.insert(mPassSetBindingTypes.end(), setBindingTypes.begin(), setBindingTypes.end());

	mDatabaseToBuild->mDescriptorSets.push_back(VK_NULL_HANDLE);
}

void Vulkan::PassDescriptorDatabaseBuilder::RecreatePassSets(const FrameGraphBuilder* frameGraphBuilder)
{
	RecreatePassDescriptorPool(frameGraphBuilder);

	std::vector<VkDescriptorSet> setsPerLayout;
	AllocateDescriptorSets(setsPerLayout);

	WriteDescriptorSets(frameGraphBuilder, setsPerLayout);
	AssignDescriptorSets(setsPerLayout);
}

void Vulkan::PassDescriptorDatabaseBuilder::RecreatePassDescriptorPool(const FrameGraphBuilder* frameGraphBuilder)
{
	SafeDestroyObject(vkDestroyDescriptorPool, mDatabaseToBuild->mDeviceRef, mDatabaseToBuild->mPassDescriptorPool);

	VkDescriptorPoolSize sampledImagePoolSize    = {.type = VK_DESCRIPTOR_TYPE_SAMPLER,        .descriptorCount = 0};
	VkDescriptorPoolSize storageImagePoolSize    = {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  .descriptorCount = 0};
	VkDescriptorPoolSize inputAttachmentPoolSize = {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 0};

	for(const PassSetInfo& passSetInfo: mPassSetInfosPerLayout)
	{
		const FrameGraphBuilder::PassMetadata& passMetadata = frameGraphBuilder->mTotalPassMetadatas[passSetInfo.PassIndex];
		for(uint32_t bindingIndex = passSetInfo.BindingIndexSpan.Begin; bindingIndex < passSetInfo.BindingIndexSpan.End; bindingIndex++)
		{
			uint32_t subresourceIndex = passMetadata.SubresourceMetadataSpan.Begin + mPassSetBindingTypes[bindingIndex];
			const SubresourceMetadataPayload& metadataPayload = frameGraphBuilder->mSubresourceMetadataPayloads[subresourceIndex];

			if(metadataPayload.Usage == VK_IMAGE_USAGE_SAMPLED_BIT)
			{
				sampledImagePoolSize.descriptorCount += 1;
			}
			else if(metadataPayload.Usage == VK_IMAGE_USAGE_STORAGE_BIT)
			{
				storageImagePoolSize.descriptorCount += 1;
			}
			else if(metadataPayload.Usage == VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
			{
				inputAttachmentPoolSize.descriptorCount += 1;
			}
		}
	}


	std::array poolSizes = {sampledImagePoolSize, storageImagePoolSize, storageImagePoolSize};

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
	descriptorPoolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext         = nullptr;
	descriptorPoolCreateInfo.flags         = 0;
	descriptorPoolCreateInfo.maxSets       = (uint32_t)mPassSetLayouts.size();
	descriptorPoolCreateInfo.poolSizeCount = (uint32_t)(poolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes    = poolSizes.data();

	ThrowIfFailed(vkCreateDescriptorPool(mDatabaseToBuild->mDeviceRef, &descriptorPoolCreateInfo, nullptr, &mDatabaseToBuild->mPassDescriptorPool));
}

void Vulkan::PassDescriptorDatabaseBuilder::AllocateDescriptorSets(std::vector<VkDescriptorSet>& outAllocatedSetsPerLayout)
{
	outAllocatedSetsPerLayout.resize(mPassSetLayouts.size());

	VkDescriptorSetAllocateInfo setAllocateInfo;
	setAllocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocateInfo.pNext              = nullptr;
	setAllocateInfo.descriptorPool     = mDatabaseToBuild->mPassDescriptorPool;
	setAllocateInfo.descriptorSetCount = (uint32_t)mPassSetLayouts.size();
	setAllocateInfo.pSetLayouts        = mPassSetLayouts.data();

	ThrowIfFailed(vkAllocateDescriptorSets(mDatabaseToBuild->mDeviceRef, &setAllocateInfo, outAllocatedSetsPerLayout.data()));
}

void Vulkan::PassDescriptorDatabaseBuilder::WriteDescriptorSets(const FrameGraphBuilder* frameGraphBuilder, const std::vector<VkDescriptorSet>& setsToWritePerLayout)
{
	std::vector<VkWriteDescriptorSet> writeDescriptorSets;
	std::vector<VkDescriptorImageInfo> descriptorImageInfos;
	for(uint32_t setIndex = 0; setIndex < mPassSetInfosPerLayout.size(); setIndex++)
	{
		const PassSetInfo& passSetInfo = mPassSetInfosPerLayout[setIndex];

		const FrameGraphBuilder::PassMetadata& passMetadata = frameGraphBuilder->mTotalPassMetadatas[passSetInfo.PassIndex];
		for(uint32_t bindingIndex = passSetInfo.BindingIndexSpan.Begin; bindingIndex < passSetInfo.BindingIndexSpan.End; bindingIndex++)
		{
			uint32_t subresourceIndex = passMetadata.SubresourceMetadataSpan.Begin + mPassSetBindingTypes[bindingIndex];

			const SubresourceMetadataPayload&                 metadataPayload  = frameGraphBuilder->mSubresourceMetadataPayloads[subresourceIndex];
			const FrameGraphBuilder::SubresourceMetadataNode& metadataNode     = frameGraphBuilder->mSubresourceMetadataNodesFlat[subresourceIndex];
			const FrameGraphBuilder::ResourceMetadata&        resourceMetadata = frameGraphBuilder->mResourceMetadatas[metadataNode.ResourceMetadataIndex];

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
			descriptorImageInfo.imageView   = frameGraphBuilder->GetRegisteredSubresource(passSetInfo.PassIndex, mPassSetBindingTypes[bindingIndex]);
			descriptorImageInfo.imageLayout = metadataPayload.Layout;
			descriptorImageInfos.push_back(descriptorImageInfo);

			VkWriteDescriptorSet writeDescriptorSet;
			writeDescriptorSet.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.pNext            = nullptr;
			writeDescriptorSet.dstSet           = setsToWritePerLayout[setIndex];
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

	vkUpdateDescriptorSets(mDatabaseToBuild->mDeviceRef, (uint32_t)writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
}

void Vulkan::PassDescriptorDatabaseBuilder::AssignDescriptorSets(const std::vector<VkDescriptorSet>& setsPerLayout)
{
	for(uint32_t setLayoutIndex = 0; setLayoutIndex < mPassSetInfosPerLayout.size(); setLayoutIndex++)
	{
		uint32_t descriptorIndex = mPassSetInfosPerLayout[setLayoutIndex].DatabaseDescriptorIndex;
		mDatabaseToBuild->mDescriptorSets[descriptorIndex] = setsPerLayout[setLayoutIndex];
	}
}