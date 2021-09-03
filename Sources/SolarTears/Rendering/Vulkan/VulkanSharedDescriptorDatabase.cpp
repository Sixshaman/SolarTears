#include "VulkanSharedDescriptorDatabase.hpp"
#include "VulkanUtils.hpp"
#include "VulkanFunctions.hpp"
#include "VulkanSamplers.hpp"
#include "Scene/VulkanScene.hpp"
#include <VulkanGenericStructures.h>

Vulkan::SharedDescriptorDatabase::SharedDescriptorDatabase(const VkDevice device): mDeviceRef(device)
{
	mDescriptorPool = VK_NULL_HANDLE;
}

Vulkan::SharedDescriptorDatabase::~SharedDescriptorDatabase()
{
	SafeDestroyObject(vkDestroyDescriptorPool, mDeviceRef, mDescriptorPool);
	for(size_t entryIndex = 0; entryIndex < mSetLayouts.size(); entryIndex++)
	{
		SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, mSetLayouts[entryIndex]);
	}
}

void Vulkan::SharedDescriptorDatabase::RecreateSets(const RenderableScene* sceneToCreateDescriptors, const SamplerManager* samplerManager)
{
	RecreateDescriptorPool(sceneToCreateDescriptors, samplerManager);

	AllocateDescriptorSets(sceneToCreateDescriptors, samplerManager);
	UpdateDescriptorSets(sceneToCreateDescriptors);
}

void Vulkan::SharedDescriptorDatabase::RecreateDescriptorPool(const RenderableScene* sceneToCreateDescriptors, const SamplerManager* samplerManager)
{
	VkDescriptorPoolSize samplerDescriptorPoolSize = {.type = VK_DESCRIPTOR_TYPE_SAMPLER,        .descriptorCount = 0};
	VkDescriptorPoolSize textureDescriptorPoolSize = {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,  .descriptorCount = 0};
	VkDescriptorPoolSize uniformDescriptorPoolSize = {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 0};

	std::array<uint32_t, TotalBindings> descriptorCountsPerBinding = std::to_array
	({
		(uint32_t)samplerManager->GetSamplerVariableArrayLength(),                                                    //Sampler descriptor count
		(uint32_t)sceneToCreateDescriptors->mSceneTextures.size(),                                                    //Texture descriptor count
		(uint32_t)(sceneToCreateDescriptors->mMaterialDataSize / sceneToCreateDescriptors->mMaterialChunkDataSize),   //Material descriptor count
		(uint32_t)(sceneToCreateDescriptors->mStaticObjectDataSize / sceneToCreateDescriptors->mObjectChunkDataSize), //Static object data descriptor count
		(uint32_t)sceneToCreateDescriptors->mCurrFrameDataToUpdate.size(),                                            //Dynamic object data descriptor count
		1u                                                                                                            //Frame data descriptor count
	});

	std::array<VkDescriptorPoolSize*, TotalBindings> poolSizeRefsPerType = std::to_array
	({
		&samplerDescriptorPoolSize, //Sampler pool size
		&textureDescriptorPoolSize, //Texture pool size
		&uniformDescriptorPoolSize, //Material pool size
		&uniformDescriptorPoolSize, //Static data pool size
		&uniformDescriptorPoolSize, //Dynamic data pool size
		&uniformDescriptorPoolSize  //Frame data pool size
	});


	uint32_t totalSetCount = 0;
	for(size_t setIndex = 0; setIndex < mSetLayouts.size(); setIndex++)
	{
		Span<uint32_t>        setBindingTypeSpan = mSetFormatsPerLayout[setIndex];
		VkDescriptorSetLayout setLayout          = mSetLayouts[setIndex];

		uint32_t setCount = mSetsPerLayout[setIndex].End - mSetsPerLayout[setIndex].Begin;
		for(uint32_t bindingIndex = setBindingTypeSpan.Begin; bindingIndex < setBindingTypeSpan.End; bindingIndex++)
		{
			uint32_t bindingTypeIndex = (uint32_t)mSetFormatsFlat[bindingIndex];
			poolSizeRefsPerType[bindingTypeIndex]->descriptorCount += descriptorCountsPerBinding[bindingTypeIndex] * setCount;
		}

		totalSetCount += setCount;
	}

	std::array poolSizes = {samplerDescriptorPoolSize, textureDescriptorPoolSize, uniformDescriptorPoolSize};

	//TODO: inline uniforms... Though do I really need them?
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
	descriptorPoolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext         = nullptr;
	descriptorPoolCreateInfo.flags         = 0;
	descriptorPoolCreateInfo.maxSets       = totalSetCount;
	descriptorPoolCreateInfo.poolSizeCount = (uint32_t)(poolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes    = poolSizes.data();

	ThrowIfFailed(vkCreateDescriptorPool(mDeviceRef, &descriptorPoolCreateInfo, nullptr, &mDescriptorPool));
}

void Vulkan::SharedDescriptorDatabase::AllocateDescriptorSets(const RenderableScene* sceneToCreateDescriptors, const SamplerManager* samplerManager)
{
	std::array<uint32_t, TotalBindings> variableCountsPerBindingType = std::to_array
	({
		(uint32_t)samplerManager->GetSamplerVariableArrayLength(),                                                    //Samplers binding
		(uint32_t)sceneToCreateDescriptors->mSceneTextures.size(),                                                    //Textures binding
		(uint32_t)(sceneToCreateDescriptors->mMaterialDataSize / sceneToCreateDescriptors->mMaterialChunkDataSize),   //Materials binding
		(uint32_t)(sceneToCreateDescriptors->mStaticObjectDataSize / sceneToCreateDescriptors->mObjectChunkDataSize), //Static object datas binding
		(uint32_t)sceneToCreateDescriptors->mCurrFrameDataToUpdate.size(),                                            //Dynamic object datas binding
		0u                                                                                                            //Frame data binding
	});

	std::vector<VkDescriptorSetLayout> layoutsForSets(mSets.size());
	std::vector<uint32_t>              variableCountsForSets(mSets.size(), 0);

	for(uint32_t layoutIndex = 0; layoutIndex < mSetLayouts.size(); layoutIndex++)
	{
		Span<uint32_t> layoutSetSpan     = mSetsPerLayout[layoutIndex];
		Span<uint32_t> layoutBindingspan = mSetFormatsPerLayout[layoutIndex];

		for(uint32_t setIndex = layoutSetSpan.Begin; setIndex < layoutSetSpan.End; setIndex++)
		{
			layoutsForSets[setIndex] = mSetLayouts[layoutIndex];
			for(size_t bindingIndex = layoutBindingspan.Begin; bindingIndex < layoutBindingspan.End; bindingIndex++)
			{
				uint32_t bindingTypeIndex = (uint32_t)mSetFormatsFlat[bindingIndex];
				uint32_t variableCount = variableCountsPerBindingType[bindingTypeIndex];

				if(variableCount != 0)
				{
					assert(variableCountsForSets[setIndex] == 0); //Only the last binding can be of variable size
					variableCountsForSets[setIndex] = variableCount;
				}
			}
		}
	}

	VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountAllocateInfo;
	variableCountAllocateInfo.pNext              = nullptr;
	variableCountAllocateInfo.descriptorSetCount = (uint32_t)variableCountsForSets.size();
	variableCountAllocateInfo.pDescriptorCounts  = variableCountsForSets.data();

	vgs::GenericStructureChain<VkDescriptorSetAllocateInfo> descriptorSetAllocateInfoChain;
	descriptorSetAllocateInfoChain.AppendToChain(variableCountAllocateInfo);

	VkDescriptorSetAllocateInfo& setAllocateInfo = descriptorSetAllocateInfoChain.GetChainHead();
	setAllocateInfo.descriptorPool     = mDescriptorPool;
	setAllocateInfo.descriptorSetCount = (uint32_t)layoutsForSets.size();
	setAllocateInfo.pSetLayouts        = layoutsForSets.data();

	ThrowIfFailed(vkAllocateDescriptorSets(mDeviceRef, &setAllocateInfo, mSets.data()));
}

void Vulkan::SharedDescriptorDatabase::UpdateDescriptorSets(const RenderableScene* sceneToCreateDescriptors)
{
	const uint32_t samplerCount      = 0; //Immutable samplers are used
	const uint32_t textureCount      = (uint32_t)sceneToCreateDescriptors->mSceneTextureViews.size();
	const uint32_t materialCount     = (uint32_t)(sceneToCreateDescriptors->mMaterialDataSize / sceneToCreateDescriptors->mMaterialChunkDataSize);
	const uint32_t staticObjectCount = (uint32_t)(sceneToCreateDescriptors->mStaticObjectDataSize / sceneToCreateDescriptors->mObjectChunkDataSize);
	const uint32_t rigidObjectCount  = (uint32_t)(sceneToCreateDescriptors->mCurrFrameDataToUpdate.size());
	const uint32_t frameDataCount    = 1u;

	std::array<uint32_t, TotalBindings> bindingDescriptorCounts = std::to_array
	({
		samplerCount,
		textureCount,
		materialCount,
		staticObjectCount,
		rigidObjectCount,
		frameDataCount
	});

	size_t imageDescriptorCount  = 0;
	size_t bufferDescriptorCount = 0;
	for(uint32_t bindingTypeIndex = 0; bindingTypeIndex < TotalBindings; bindingTypeIndex++)
	{
		size_t frameCount   = FrameCountsPerBinding[bindingTypeIndex];
		size_t bindingCount = bindingDescriptorCounts[bindingTypeIndex];

		switch(DescriptorTypesPerBinding[bindingTypeIndex])
		{
		case VK_DESCRIPTOR_TYPE_SAMPLER:
			break;

		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			imageDescriptorCount += bindingCount * frameCount;
			break;

		case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			bufferDescriptorCount += bindingCount * frameCount;
			break;

		default:
			assert(false); //Unimplemented
			break;
		}
	}

	std::vector<VkDescriptorImageInfo>  imageDescriptorInfos;
	std::vector<VkDescriptorBufferInfo> bufferDescriptorInfos;

	constexpr auto addSamplerBindingsFunc = [](const RenderableScene* sceneToCreateDescriptors, std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, uint32_t samplerIndex, uint32_t bindingFrameIndex)
	{
		//Do nothing
	};

	constexpr auto addTextureBindingFunc = [](const RenderableScene* sceneToCreateDescriptors, std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, uint32_t textureIndex, uint32_t bindingFrameIndex)
	{
		imageDescriptorInfos.push_back(VkDescriptorImageInfo
		{
			.sampler     = VK_NULL_HANDLE,
			.imageView   = sceneToCreateDescriptors->mSceneTextureViews[textureIndex],
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		});
	};

	constexpr auto addMaterialBindingsFunc = [](const RenderableScene* sceneToCreateDescriptors, std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, uint32_t materialIndex, uint32_t bindingFrameIndex)
	{
		bufferDescriptorInfos.push_back(VkDescriptorBufferInfo
		{
			.buffer = sceneToCreateDescriptors->mSceneStaticUniformBuffer,
			.offset = sceneToCreateDescriptors->CalculateMaterialDataOffset(materialIndex),
			.range  = sceneToCreateDescriptors->mMaterialChunkDataSize
		});
	};

	constexpr auto addStaticObjectBindingsFunc = [](const RenderableScene* sceneToCreateDescriptors, std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, uint32_t staticObjectIndex, uint32_t bindingFrameIndex)
	{
		bufferDescriptorInfos.push_back(VkDescriptorBufferInfo
		{
			.buffer = sceneToCreateDescriptors->mSceneStaticUniformBuffer,
			.offset = sceneToCreateDescriptors->CalculateStaticObjectDataOffset(staticObjectIndex),
			.range  = sceneToCreateDescriptors->mObjectChunkDataSize
		});
	};

	constexpr auto addRigidObjectBindingsFunc = [](const RenderableScene* sceneToCreateDescriptors, std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, uint32_t rigidObjectIndex, uint32_t bindingFrameIndex)
	{
		bufferDescriptorInfos.push_back(VkDescriptorBufferInfo
		{
			.buffer = sceneToCreateDescriptors->mSceneDynamicUniformBuffer,
			.offset = sceneToCreateDescriptors->CalculateRigidObjectDataOffset(bindingFrameIndex, rigidObjectIndex),
			.range  = sceneToCreateDescriptors->mObjectChunkDataSize
		});
	};

	constexpr auto addFrameDataBindingsFunc = [](const RenderableScene* sceneToCreateDescriptors, std::vector<VkDescriptorImageInfo>& imageDescriptorInfos, std::vector<VkDescriptorBufferInfo>& bufferDescriptorInfos, uint32_t bindingIndex, uint32_t bindingFrameIndex)
	{
		bufferDescriptorInfos.push_back(VkDescriptorBufferInfo
		{
			.buffer = sceneToCreateDescriptors->mSceneDynamicUniformBuffer,
			.offset = sceneToCreateDescriptors->CalculateFrameDataOffset(bindingFrameIndex),
			.range  = sceneToCreateDescriptors->mFrameChunkDataSize
		});
	};

	using AddBindingFunc = void(*)(const RenderableScene*, std::vector<VkDescriptorImageInfo>&, std::vector<VkDescriptorBufferInfo>&, uint32_t, uint32_t);
	constexpr std::array<AddBindingFunc, TotalBindings> bindingAddFuncs = std::to_array
	({
		(AddBindingFunc)addSamplerBindingsFunc,
		(AddBindingFunc)addTextureBindingFunc,
		(AddBindingFunc)addMaterialBindingsFunc,
		(AddBindingFunc)addStaticObjectBindingsFunc,
		(AddBindingFunc)addRigidObjectBindingsFunc,
		(AddBindingFunc)addFrameDataBindingsFunc
	});

	constexpr uint32_t uniqueBindingCount = std::accumulate(FrameCountsPerBinding.begin(), FrameCountsPerBinding.end(), 0u);

	constexpr std::array<uint32_t, TotalBindings> uniqueBindingStarts = { 0 };
	std::exclusive_scan(FrameCountsPerBinding.begin(), FrameCountsPerBinding.end(), uniqueBindingStarts.begin(), 0u);

	std::array<VkDescriptorImageInfo*,  uniqueBindingCount> imageDescriptorInfosPerUniqueBinding       = {0};
	std::array<VkDescriptorBufferInfo*, uniqueBindingCount> bufferDescriptorInfosPerUniqueBinding      = {0};
	std::array<VkBufferView*,           uniqueBindingCount> texelBufferDescriptorInfosPerUniqueBinding = {0};

	imageDescriptorInfos.reserve(imageDescriptorCount);
	bufferDescriptorInfos.reserve(bufferDescriptorCount);
	for(uint32_t bindingTypeIndex = 0, totalBindingIndex = 0; bindingTypeIndex < TotalBindings; bindingTypeIndex++)
	{
		AddBindingFunc addfunc      = bindingAddFuncs[bindingTypeIndex];
		uint32_t       bindingCount = bindingDescriptorCounts[bindingTypeIndex];
		for(uint32_t frameIndex = 0; frameIndex < FrameCountsPerBinding[bindingTypeIndex]; frameIndex++, totalBindingIndex++)
		{
			//The memory is already reserved, it's safe to assume it won't move anywhere
			imageDescriptorInfosPerUniqueBinding[totalBindingIndex]       = imageDescriptorInfos.data()  + imageDescriptorInfos.size();
			bufferDescriptorInfosPerUniqueBinding[totalBindingIndex]      = bufferDescriptorInfos.data() + bufferDescriptorInfos.size();
			texelBufferDescriptorInfosPerUniqueBinding[totalBindingIndex] = nullptr;

			for(uint32_t bindingIndex = 0; bindingIndex < bindingCount; bindingIndex++)
			{
				addfunc(sceneToCreateDescriptors, imageDescriptorInfos, bufferDescriptorInfos, bindingIndex, frameIndex);
			}
		}
	}

	uint32_t allBindingCount = std::inner_product(mSetFormatsPerLayout.begin(), mSetFormatsPerLayout.end(), mSetsPerLayout.begin(), 0, std::plus<>(), [](const Span<uint32_t>& bindingSpan, const Span<uint32_t>& setSpan)
	{
		return (bindingSpan.End - bindingSpan.Begin) * (setSpan.End - setSpan.Begin);
	});

	std::vector<VkWriteDescriptorSet> writeDescriptorSets;
	writeDescriptorSets.reserve(allBindingCount);

	for(uint32_t layoutIndex = 0; layoutIndex < mSetLayouts.size(); layoutIndex++)
	{
		Span<uint32_t> layoutSetSpan     = mSetsPerLayout[layoutIndex];
		Span<uint32_t> layoutBindingSpan = mSetFormatsPerLayout[layoutIndex];

		for(uint32_t bindingIndex = layoutBindingSpan.Begin; bindingIndex < layoutBindingSpan.End; bindingIndex++)
		{
			for(uint32_t setIndex = layoutSetSpan.Begin; setIndex < layoutSetSpan.End; setIndex++)
			{
				uint32_t layoutBindingIndex = bindingIndex - layoutBindingSpan.Begin;
				uint32_t bindingTypeIndex   = (uint32_t)mSetFormatsFlat[bindingIndex];

				uint32_t uniqueBindingIndex = uniqueBindingStarts[bindingIndex] + setIndex;

				VkWriteDescriptorSet writeDescriptorSet;
				writeDescriptorSet.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSet.pNext            = nullptr;
				writeDescriptorSet.dstSet           = mSets[setIndex];
				writeDescriptorSet.dstBinding       = layoutBindingIndex;
				writeDescriptorSet.dstArrayElement  = 0;
				writeDescriptorSet.descriptorCount  = bindingDescriptorCounts[bindingTypeIndex];
				writeDescriptorSet.descriptorType   = DescriptorTypesPerBinding[bindingTypeIndex];
				writeDescriptorSet.pImageInfo       = imageDescriptorInfosPerUniqueBinding[uniqueBindingIndex];
				writeDescriptorSet.pBufferInfo      = bufferDescriptorInfosPerUniqueBinding[uniqueBindingIndex];
				writeDescriptorSet.pTexelBufferView = texelBufferDescriptorInfosPerUniqueBinding[uniqueBindingIndex];

				writeDescriptorSets.push_back(writeDescriptorSet);
			}
		}
	}

	vkUpdateDescriptorSets(mDeviceRef, (uint32_t)writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
}