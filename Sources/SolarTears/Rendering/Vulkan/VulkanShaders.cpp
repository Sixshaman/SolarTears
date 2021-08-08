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
	CreateSamplers();
}

Vulkan::ShaderManager::~ShaderManager()
{
	for(size_t i = 0; i < mSamplers.size(); i++)
	{
		SafeDestroyObject(vkDestroySampler, mDeviceRef, mSamplers[i]);
	}
}

spv_reflect::ShaderModule Vulkan::ShaderManager::LoadShaderBlob(const std::wstring& path) const
{
	std::vector<uint32_t> shaderData;
	VulkanUtils::LoadShaderModuleFromFile(path, shaderData, mLogger);

	return spv_reflect::ShaderModule(shaderData);
}

void Vulkan::ShaderManager::FindBindings(const std::span<spv_reflect::ShaderModule*>& shaderModules, std::vector<VkDescriptorSetLayoutBinding>& outSetBindings, std::vector<std::string>& outBindingNames, std::vector<Span<uint32_t>>& outBindingSpans) const
{	
	outSetBindings.clear();
	outBindingNames.clear();
	outBindingSpans.clear();

	std::vector<std::vector<SpvReflectDescriptorBinding*>> bindingsForSets;
	std::vector<std::vector<VkShaderStageFlags>>           bindingsStageFlags;
	GatherSetBindings(shaderModules, bindingsForSets, bindingsStageFlags);

	for(size_t setIndex = 0; setIndex < bindingsForSets.size(); setIndex++)
	{
		const std::vector<SpvReflectDescriptorBinding*>& setBindings           = bindingsForSets[setIndex];
		const std::vector<VkShaderStageFlags>&           setBindingsStageFlags = bindingsStageFlags[setIndex];

		Span<uint32_t> bindingSpan;
		bindingSpan.Begin = (uint32_t)outSetBindings.size();
		bindingSpan.End   = (uint32_t)(outSetBindings.size() + setBindings.size());

		for(size_t bindingIndex = 0; bindingIndex < setBindings.size(); bindingIndex++)
		{
			SpvReflectDescriptorBinding* setBinding       = setBindings[bindingIndex];
			VkShaderStageFlags           shaderStageFlags = setBindingsStageFlags[bindingIndex];

			const VkSampler* descriptorImmutableSamplers = nullptr;

			VkDescriptorType descriptorType = SpvToVkDescriptorType(setBinding->descriptor_type);
			if (descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER || descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
			{
				descriptorImmutableSamplers = mSamplers.data();
			}

			uint32_t descriptorCount = setBinding->count;
			if(setBinding->type_description->op == SpvOpTypeRuntimeArray)
			{
				descriptorCount = (uint32_t)(-1); //A way to check for variable descriptor count
			}

			VkDescriptorSetLayoutBinding descriptorSetBinding =
			{
				.binding            = setBinding->binding,
				.descriptorType     = descriptorType,
				.descriptorCount    = descriptorCount,
				.stageFlags         = shaderStageFlags,
				.pImmutableSamplers = descriptorImmutableSamplers
			};
		
			outSetBindings.push_back(descriptorSetBinding);
			outBindingNames.push_back(setBinding->name);
		}

		outBindingSpans.push_back(bindingSpan);
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
	std::array samplerAnisotropyFlags  = {VK_FALSE, VK_TRUE};
	std::array samplerAnisotropyLevels = {0.0f,     1.0f};

	constexpr size_t samplerCount = std::tuple_size<decltype(mSamplers)>::value;
	static_assert(samplerCount == samplerAnisotropyFlags.size());
	static_assert(samplerCount == samplerAnisotropyLevels.size());

	for(size_t i = 0; i < mSamplers.size(); i++)
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

		ThrowIfFailed(vkCreateSampler(mDeviceRef, &samplerCreateInfo, nullptr, &mSamplers[i]));
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

			//Name should be defined in every shader for every uniform block
			assert(descriptorBinding->name != nullptr && descriptorBinding->name[0] != '\0');

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