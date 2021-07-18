#include "VulkanShaders.hpp"
#include "VulkanUtils.hpp"
#include "VulkanFunctions.hpp"
#include "../../Core/Util.hpp"
#include "../../Logging/Logger.hpp"
#include <VulkanGenericStructures.h>
#include <cassert>
#include <unordered_map>
#include <array>

Vulkan::ShaderManager::ShaderManager(LoggerQueue* logger, VkDevice device): mDeviceRef(device), mLogger(logger)
{
	mLinearSampler      = VK_NULL_HANDLE;
	mAnisotropicSampler = VK_NULL_HANDLE;

	CreateSamplers();
}

Vulkan::ShaderManager::~ShaderManager()
{
	SafeDestroyObject(vkDestroySampler, mDeviceRef, mLinearSampler);
	SafeDestroyObject(vkDestroySampler, mDeviceRef, mAnisotropicSampler);
}

spv_reflect::ShaderModule Vulkan::ShaderManager::LoadShaderBlob(const std::wstring& path) const
{
	std::vector<uint32_t> shaderData;
	VulkanUtils::LoadShaderModuleFromFile(path, shaderData, mLogger);

	return spv_reflect::ShaderModule(shaderData);
}

void Vulkan::ShaderManager::CreatePipelineLayout(VkDevice device, const std::span<spv_reflect::ShaderModule*>& shaderModules, VkPipelineLayout* outPipelineLayout) const
{	
	std::vector<std::vector<SpvReflectDescriptorBinding*>> bindingsForSets;
	std::vector<std::vector<VkShaderStageFlags>>           bindingsStageFlags;
	GatherSetBindings(shaderModules, bindingsForSets, bindingsStageFlags);

	std::array immutableSamplers = {mLinearSampler, mAnisotropicSampler};

	std::vector<VkDescriptorSetLayout> descriptorSetLayouts(bindingsForSets.size());
	for(size_t setIndex = 0; setIndex < bindingsForSets.size(); setIndex++)
	{
		std::vector<VkDescriptorSetLayoutBinding> setBindings;
		std::vector<VkDescriptorBindingFlags>     setFlags;
		BuildDescriptorBindingInfos(bindingsForSets[setIndex], bindingsStageFlags[setIndex], immutableSamplers, setBindings, setFlags);

		//Need this for descriptor indexing
		vgs::StructureChainBlob<VkDescriptorSetLayoutCreateInfo> descriptorSetLayoutCreateInfoChain;
		descriptorSetLayoutCreateInfoChain.AppendToChain(VkDescriptorSetLayoutBindingFlagsCreateInfo
		{
			.bindingCount  = (uint32_t)setFlags.size(),
			.pBindingFlags = setFlags.data()
		});
	
		VkDescriptorSetLayoutCreateInfo& setLayoutCreateInfo = descriptorSetLayoutCreateInfoChain.GetChainHead();
		setLayoutCreateInfo.flags        = 0;
		setLayoutCreateInfo.bindingCount = (uint32_t)setBindings.size();
		setLayoutCreateInfo.pBindings    = setBindings.data();

		ThrowIfFailed(vkCreateDescriptorSetLayout(mDeviceRef, &setLayoutCreateInfo, nullptr, &descriptorSetLayouts[setIndex]));
	}
}

VkShaderStageFlagBits Vulkan::ShaderManager::SpvToVkShaderStage(SpvReflectShaderStageFlagBits spvShaderStage) const
{
	switch (spvShaderStage)
	{
	case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
		return VK_SHADER_STAGE_VERTEX_BIT;

	case SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
		return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;

	case SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
		return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

	case SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT:
		return VK_SHADER_STAGE_GEOMETRY_BIT;

	case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
		return VK_SHADER_STAGE_FRAGMENT_BIT;

	case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
		return VK_SHADER_STAGE_COMPUTE_BIT;

	case SPV_REFLECT_SHADER_STAGE_TASK_BIT_NV:
		return VK_SHADER_STAGE_TASK_BIT_NV;

	case SPV_REFLECT_SHADER_STAGE_MESH_BIT_NV:
		return VK_SHADER_STAGE_MESH_BIT_NV;

	case SPV_REFLECT_SHADER_STAGE_RAYGEN_BIT_KHR:
		return VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	case SPV_REFLECT_SHADER_STAGE_ANY_HIT_BIT_KHR:
		return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;

	case SPV_REFLECT_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
		return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

	case SPV_REFLECT_SHADER_STAGE_MISS_BIT_KHR:
		return VK_SHADER_STAGE_MISS_BIT_KHR;

	case SPV_REFLECT_SHADER_STAGE_INTERSECTION_BIT_KHR:
		return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

	case SPV_REFLECT_SHADER_STAGE_CALLABLE_BIT_KHR:
		return VK_SHADER_STAGE_CALLABLE_BIT_KHR;

	default:
		return VK_SHADER_STAGE_ALL;
	}
}

VkDescriptorType Vulkan::ShaderManager::SpvToVkDescriptorType(SpvReflectDescriptorType spvDescriptorType) const
{
	switch (spvDescriptorType)
	{
	case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
		return VK_DESCRIPTOR_TYPE_SAMPLER;

	case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
		return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

	case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

	case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
		return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;

	case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
		return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;

	case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

	case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;

	case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;

	case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
		return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;

	case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
		return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

	default:
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	}
}

void Vulkan::ShaderManager::CreateSamplers()
{
	std::array samplerPointers         = {&mLinearSampler, &mAnisotropicSampler};
	std::array samplerAnisotropyFlags  = {VK_FALSE,        VK_TRUE};
	std::array samplerAnisotropyLevels = {0.0f,            1.0f};

	static_assert(samplerPointers.size() == samplerAnisotropyFlags.size());
	static_assert(samplerPointers.size() == samplerAnisotropyLevels.size());

	for(size_t i = 0; i < samplerPointers.size(); i++)
	{
		//TODO: Fragment density map flags
		//TODO: Cubic filter
		VkSamplerCreateInfo samplerCreateInfo;
		samplerCreateInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.pNext                   = nullptr;
		samplerCreateInfo.flags                   = 0;
		samplerCreateInfo.magFilter               = VK_FILTER_LINEAR;
		samplerCreateInfo.minFilter               = VK_FILTER_LINEAR;
		samplerCreateInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCreateInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCreateInfo.mipLodBias              = 0.0f;
		samplerCreateInfo.anisotropyEnable        = samplerAnisotropyFlags[i];
		samplerCreateInfo.maxAnisotropy           = samplerAnisotropyLevels[i];
		samplerCreateInfo.compareEnable           = VK_FALSE;
		samplerCreateInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
		samplerCreateInfo.minLod                  = 0.0f;
		samplerCreateInfo.maxLod                  = FLT_MAX;
		samplerCreateInfo.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

		ThrowIfFailed(vkCreateSampler(mDeviceRef, &samplerCreateInfo, nullptr, samplerPointers[i]));
	}
}

uint32_t Vulkan::ShaderManager::GetModulesSetCount(const std::span<spv_reflect::ShaderModule*>& shaderModules) const
{
	uint32_t totalSetCount = 0;
	for(spv_reflect::ShaderModule* shaderModule: shaderModules)
	{
		uint32_t moduleSetCount = 0;
		shaderModule->EnumerateDescriptorSets(&moduleSetCount, nullptr);

		std::vector<SpvReflectDescriptorSet*> moduleSets(moduleSetCount);
		shaderModule->EnumerateDescriptorSets(&moduleSetCount, moduleSets.data());

		for(SpvReflectDescriptorSet* descriptorSet: moduleSets)
		{
			totalSetCount = std::max(totalSetCount, descriptorSet->set + 1);
		}
	}

	return totalSetCount;
}

void Vulkan::ShaderManager::GatherSetBindings(const std::span<spv_reflect::ShaderModule*>& shaderModules, std::vector<std::vector<SpvReflectDescriptorBinding*>>& outBindingsForSets, std::vector<std::vector<VkShaderStageFlags>>& outBindingsStageFlags) const
{
	uint32_t setCount = GetModulesSetCount(shaderModules);

	outBindingsForSets.resize(setCount);
	outBindingsStageFlags.resize(setCount);
	for(spv_reflect::ShaderModule* shaderModule: shaderModules)
	{
		uint32_t moduleBindingCount = 0;
		shaderModule->EnumerateDescriptorBindings(&moduleBindingCount, nullptr);

		std::vector<SpvReflectDescriptorBinding*> moduleBindings(moduleBindingCount);
		shaderModule->EnumerateDescriptorBindings(&moduleBindingCount, moduleBindings.data());

		VkShaderStageFlags shaderStageFlags = SpvToVkShaderStage(shaderModule->GetShaderStage());
		for(SpvReflectDescriptorBinding* descriptorBinding: moduleBindings)
		{
			std::vector<SpvReflectDescriptorBinding*>& setBindings          = outBindingsForSets[descriptorBinding->set];
			std::vector<VkShaderStageFlags>&           setBindingStageFlags = outBindingsStageFlags[descriptorBinding->set];

			if(descriptorBinding->binding >= setBindings.size())
			{
				setBindings.resize(descriptorBinding->binding + 1, nullptr);
				setBindings[descriptorBinding->binding] = descriptorBinding;

				setBindingStageFlags.resize(descriptorBinding->binding + 1, 0);
				setBindingStageFlags[descriptorBinding->binding] = shaderStageFlags;
			}
			else
			{
				if(setBindings[descriptorBinding->binding] == nullptr)
				{
					setBindings[descriptorBinding->binding]          = descriptorBinding;
					setBindingStageFlags[descriptorBinding->binding] = shaderStageFlags;
				}
				else
				{
					//Make sure the binding is the same
					SpvReflectDescriptorBinding* definedBinding = setBindings[descriptorBinding->binding];

					assert(strcmp(descriptorBinding->name, definedBinding->name) == 0);
					assert(descriptorBinding->resource_type == definedBinding->resource_type);
					assert(descriptorBinding->count         == definedBinding->count);

					setBindingStageFlags[descriptorBinding->binding] |= shaderStageFlags;
				}
			}
		}
	}
}

void Vulkan::ShaderManager::BuildDescriptorBindingInfos(const std::span<SpvReflectDescriptorBinding*> spvSetBindings, const std::span<VkShaderStageFlags> setBindingStageFlags, const std::span<VkSampler> immutableSamplers, std::vector<VkDescriptorSetLayoutBinding>& outSetBindings, std::vector<VkDescriptorBindingFlags>& outSetFlags) const
{
	outSetBindings.resize(spvSetBindings.size());
	outSetFlags.resize(spvSetBindings.size(), 0);

	for(uint32_t bindingIndex = 0; bindingIndex < spvSetBindings.size(); bindingIndex++)
	{
		SpvReflectDescriptorBinding* setBinding       = spvSetBindings[bindingIndex];
		VkShaderStageFlags           shaderStageFlags = setBindingStageFlags[bindingIndex];

		VkSampler* descriptorImmutableSamplers = nullptr;

		VkDescriptorType descriptorType = SpvToVkDescriptorType(setBinding->descriptor_type);
		if (descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER || descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
		{
			descriptorImmutableSamplers = immutableSamplers.data();
		}

		outSetBindings[bindingIndex].binding            = setBinding->binding;
		outSetBindings[bindingIndex].descriptorType     = descriptorType;
		outSetBindings[bindingIndex].descriptorCount    = setBinding->count;
		outSetBindings[bindingIndex].stageFlags         = shaderStageFlags;
		outSetBindings[bindingIndex].pImmutableSamplers = descriptorImmutableSamplers;

		if(setBinding->type_description->op == SpvOpTypeRuntimeArray)
		{
			outSetFlags[bindingIndex] |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
		}
	}
}