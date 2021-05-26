#include "VulkanSceneDescriptorCreator.hpp"
#include "../VulkanShaders.hpp"
#include "../VulkanDescriptorManager.hpp"
#include "../VulkanFunctions.hpp"
#include "../VulkanUtils.hpp"
#include <array>

void Vulkan::SceneDescriptorCreator::RecreateSceneDescriptors(VkDevice device)
{
}

void Vulkan::SceneDescriptorCreator::CreateDescriptorPool() //œ”Àﬂ ƒ≈— –»œ“Œ–Œ¬! œ¿”!
{	
	std::unordered_map<VkDescriptorType, uint32_t> descriptorTypeCounts;

	descriptorTypeCounts[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER]         = 0;
	descriptorTypeCounts[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC] = 0;
	descriptorTypeCounts[VK_DESCRIPTOR_TYPE_SAMPLER]                = 0;
	descriptorTypeCounts[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] = 0;
	descriptorTypeCounts[VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE]          = 0;

	descriptorTypeCounts.at(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) += (uint32_t)(mSceneToCreateDescriptors->mSceneTextures.size());
	descriptorTypeCounts.at(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) += 2; //Per-frame and per-object uniform buffers

	uint32_t maxDescriptorSets = 0;
	std::vector<VkDescriptorPoolSize> descriptorPoolSizes;
	for(auto it = descriptorTypeCounts.begin(); it != descriptorTypeCounts.end(); it++)
	{
		if(it->second != 0)
		{
			VkDescriptorPoolSize descriptorPoolSize;
			descriptorPoolSize.type            = it->first;
			descriptorPoolSize.descriptorCount = it->second;
			descriptorPoolSizes.push_back(descriptorPoolSize);

			maxDescriptorSets += it->second;
		}
	}

	//TODO: inline uniforms
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
	descriptorPoolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext         = nullptr;
	descriptorPoolCreateInfo.flags         = 0;
	descriptorPoolCreateInfo.maxSets       = maxDescriptorSets;
	descriptorPoolCreateInfo.poolSizeCount = (uint32_t)(descriptorPoolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes    = descriptorPoolSizes.data();

	ThrowIfFailed(vkCreateDescriptorPool(mSceneToCreateDescriptors->mDeviceRef, &descriptorPoolCreateInfo, nullptr, &mSceneToCreateDescriptors->mDescriptorPool));
}

void Vulkan::SceneDescriptorCreator::AllocateDescriptorSets(const DescriptorManager* descriptorManager)
{
	std::array bufferDescriptorSetLayouts = {descriptorManager->GetGBufferUniformsDescriptorSetLayout()};

	//TODO: variable descriptor count
	VkDescriptorSetAllocateInfo bufferDescriptorSetAllocateInfo;
	bufferDescriptorSetAllocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	bufferDescriptorSetAllocateInfo.pNext              = nullptr;
	bufferDescriptorSetAllocateInfo.descriptorPool     = mSceneToCreateDescriptors->mDescriptorPool;
	bufferDescriptorSetAllocateInfo.descriptorSetCount = (uint32_t)(bufferDescriptorSetLayouts.size());
	bufferDescriptorSetAllocateInfo.pSetLayouts        = bufferDescriptorSetLayouts.data();

	std::array bufferDescriptorSets = {mSceneToCreateDescriptors->mGBufferUniformsDescriptorSet};
	ThrowIfFailed(vkAllocateDescriptorSets(mSceneToCreateDescriptors->mDeviceRef, &bufferDescriptorSetAllocateInfo, bufferDescriptorSets.data()));
	mSceneToCreateDescriptors->mGBufferUniformsDescriptorSet = bufferDescriptorSets[0];


	std::vector<VkDescriptorSetLayout> textureDescriptorSetLayouts(mSceneToCreateDescriptors->mSceneTextures.size());
	for(size_t i = 0; i < mSceneToCreateDescriptors->mSceneTextures.size(); i++)
	{
		textureDescriptorSetLayouts[i] = descriptorManager->GetGBufferTexturesDescriptorSetLayout();
	}

	//TODO: variable descriptor count
	VkDescriptorSetAllocateInfo textureDescriptorSetAllocateInfo;
	textureDescriptorSetAllocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	textureDescriptorSetAllocateInfo.pNext              = nullptr;
	textureDescriptorSetAllocateInfo.descriptorPool     = mSceneToCreateDescriptors->mDescriptorPool;
	textureDescriptorSetAllocateInfo.descriptorSetCount = (uint32_t)(textureDescriptorSetLayouts.size());
	textureDescriptorSetAllocateInfo.pSetLayouts        = textureDescriptorSetLayouts.data();

	mSceneToCreateDescriptors->mGBufferTextureDescriptorSets.resize(mSceneToCreateDescriptors->mSceneTextures.size());
	ThrowIfFailed(vkAllocateDescriptorSets(mSceneToCreateDescriptors->mDeviceRef, &textureDescriptorSetAllocateInfo, mSceneToCreateDescriptors->mGBufferTextureDescriptorSets.data()));
}

void Vulkan::SceneDescriptorCreator::FillDescriptorSets(const ShaderManager* shaderManager)
{
	std::vector<VkWriteDescriptorSet> writeDescriptorSets;
	std::vector<VkCopyDescriptorSet>  copyDescriptorSets;

	VkDeviceSize uniformObjectDataSize = mSceneToCreateDescriptors->mScenePerObjectData.size() * mSceneToCreateDescriptors->mGBufferObjectChunkDataSize;
	VkDeviceSize uniformFrameDataSize  = mSceneToCreateDescriptors->mGBufferFrameChunkDataSize;

	std::array gbufferUniformBindingNames = {"UniformBufferObject",                                           "UniformBufferFrame"};
	std::array gbufferUniformOffsets      = {mSceneToCreateDescriptors->mSceneDataConstantObjectBufferOffset, mSceneToCreateDescriptors->mSceneDataConstantFrameBufferOffset};
	std::array gbufferUniformRanges       = {uniformObjectDataSize,                                           uniformFrameDataSize};

	uint32_t minUniformBindingNumber = UINT32_MAX; //Uniform buffer descriptor set has sequentional bindings

	std::array<VkDescriptorBufferInfo, gbufferUniformBindingNames.size()> gbufferDescriptorBufferInfos;
	for(size_t i = 0; i < gbufferUniformBindingNames.size(); i++)
	{
		uint32_t bindingNumber = 0;
		shaderManager->GetGBufferDrawDescriptorBindingInfo(gbufferUniformBindingNames[i], &bindingNumber, nullptr, nullptr, nullptr, nullptr);

		minUniformBindingNumber = std::min(minUniformBindingNumber, bindingNumber);

		gbufferDescriptorBufferInfos[i].buffer = mSceneToCreateDescriptors->mSceneUniformBuffer;
		gbufferDescriptorBufferInfos[i].offset = gbufferUniformOffsets[i];
		gbufferDescriptorBufferInfos[i].range  = gbufferUniformRanges[i];
	}

	//TODO: inline uniforms (and raytracing!)
	VkWriteDescriptorSet uniformWriteDescriptor;
	uniformWriteDescriptor.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	uniformWriteDescriptor.pNext            = nullptr;
	uniformWriteDescriptor.dstSet           = mSceneToCreateDescriptors->mGBufferUniformsDescriptorSet;
	uniformWriteDescriptor.dstBinding       = minUniformBindingNumber; //Uniform buffer descriptor set is sequentional
	uniformWriteDescriptor.dstArrayElement  = 0; //Uniform buffers have only 1 array element
	uniformWriteDescriptor.descriptorCount  = (uint32_t)gbufferDescriptorBufferInfos.size();
	uniformWriteDescriptor.descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	uniformWriteDescriptor.pImageInfo       = nullptr;
	uniformWriteDescriptor.pBufferInfo      = gbufferDescriptorBufferInfos.data();
	uniformWriteDescriptor.pTexelBufferView = nullptr;

	writeDescriptorSets.push_back(uniformWriteDescriptor);

	std::vector<VkDescriptorImageInfo> descriptorImageInfos;
	for(size_t i = 0; i < mSceneToCreateDescriptors->mSceneTextures.size(); i++)
	{
		uint32_t         bindingNumber  = 0;
		uint32_t         arraySize      = 0;
		VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
		shaderManager->GetGBufferDrawDescriptorBindingInfo("ObjectTexture", &bindingNumber, nullptr, &arraySize, nullptr, &descriptorType);

		assert(arraySize == 1); //For now only 1 descriptor is supported

		VkDescriptorImageInfo descriptorImageInfo;
		descriptorImageInfo.imageView   = mSceneToCreateDescriptors->mSceneTextureViews[i];
		descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		descriptorImageInfo.sampler     = nullptr; //Immutable samplers are used
		descriptorImageInfos.push_back(descriptorImageInfo);

		//TODO: inline uniforms (and raytracing!)
		VkWriteDescriptorSet textureWriteDescriptor;
		textureWriteDescriptor.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		textureWriteDescriptor.pNext            = nullptr;
		textureWriteDescriptor.dstSet           = mSceneToCreateDescriptors->mGBufferTextureDescriptorSets[i];
		textureWriteDescriptor.dstBinding       = bindingNumber;
		textureWriteDescriptor.dstArrayElement  = 0;
		textureWriteDescriptor.descriptorCount  = arraySize;
		textureWriteDescriptor.descriptorType   = descriptorType;
		textureWriteDescriptor.pImageInfo       = &descriptorImageInfos.back();
		textureWriteDescriptor.pBufferInfo      = nullptr;
		textureWriteDescriptor.pTexelBufferView = nullptr;

		writeDescriptorSets.push_back(textureWriteDescriptor);
	}

	vkUpdateDescriptorSets(mSceneToCreateDescriptors->mDeviceRef, (uint32_t)(writeDescriptorSets.size()), writeDescriptorSets.data(), (uint32_t)(copyDescriptorSets.size()), copyDescriptorSets.data());
}
