#include "VulkanShaders.hpp"
#include "VulkanUtils.hpp"
#include "VulkanFunctions.hpp"
#include "../../Core/Util.hpp"
#include "../../Logging/Logger.hpp"
#include "VulkanSamplerDescriptorDatabase.hpp"
#include "Scene/VulkanSceneDescriptorDatabase.hpp"
#include "FrameGraph/VulkanPassDescriptorDatabase.hpp"
#include <VulkanGenericStructures.h>
#include <cassert>
#include <unordered_map>
#include <array>

Vulkan::ShaderDatabase::ShaderDatabase(LoggerQueue* logger): mLogger(logger)
{
}

Vulkan::ShaderDatabase::~ShaderDatabase()
{
}

void Vulkan::ShaderDatabase::RegisterShaderGroup(const std::string& groupName, RegisterGroupInfo& registerInfo, std::span<std::wstring> shaderPaths, ShaderGroupRegisterFlags groupRegisterFlags)
{
	for(const std::wstring& shaderPath: shaderPaths)
	{
		if(!mLoadedShaderModules.contains(shaderPath))
		{
			std::vector<uint32_t> shaderData;
			VulkanUtils::LoadShaderModuleFromFile(shaderPath, shaderData, mLogger);

			mLoadedShaderModules.emplace(std::make_pair(shaderPath, spv_reflect::ShaderModule(shaderData)));
		}

		std::vector<VkDescriptorSetLayoutBinding>            setBindings;
		std::vector<std::string>                             setBindingNames;
		std::vector<TypedSpan<uint32_t, VkShaderStageFlags>> setSpans;
		FindBindings(shaderPaths, setBindings, setBindingNames, setSpans);

		for(TypedSpan<uint32_t, VkShaderStageFlags> setSpan: setSpans)
		{
			std::span<VkDescriptorSetLayoutBinding> bindingSpan = {setBindings.begin()     + setSpan.Begin, setBindings.begin()     + setSpan.End};
			std::span<std::string>                  nameSpan    = {setBindingNames.begin() + setSpan.Begin, setBindingNames.begin() + setSpan.End};
			if(registerInfo.SamplerDatabase->IsSamplerDescriptorSet(bindingSpan, nameSpan))
			{
				registerInfo.SamplerDatabase->RegisterSamplerSet(setSpan.Type);
			}
			else
			{
				SceneDescriptorSetType sceneSetType = registerInfo.SceneDatabase->ComputeSetType(bindingSpan, nameSpan);
				assert(sceneSetType != SceneDescriptorSetType::Unknown);

				if(sceneSetType != SceneDescriptorSetType::Unknown)
				{
					registerInfo.SceneDatabase->RegisterRequiredSet(sceneSetType, setSpan.Type);
				}
				else
				{
					uint32_t setId = registerInfo.PassBindingsValidateFunc(bindingSpan, nameSpan);
					assert(setId != (uint32_t)(-1));

					registerInfo.PassDatabase->RegisterRequiredSet(registerInfo.PassName, setId, setSpan.Type);
				}
			}
		}
	}
}

void Vulkan::ShaderDatabase::GetRegisteredShaderInfo(const std::wstring& path, const uint32_t** outShaderData, uint32_t* outShaderSize) const
{
	assert(outShaderData != nullptr);
	assert(outShaderSize != nullptr);

	const spv_reflect::ShaderModule& shaderModule = mLoadedShaderModules.at(path);

	*outShaderData = shaderModule.GetCode();
	*outShaderSize = shaderModule.GetCodeSize();
}

void Vulkan::ShaderDatabase::FindBindings(const std::span<std::wstring> shaderModuleNames, std::vector<VkDescriptorSetLayoutBinding>& outSetBindings, std::vector<std::string>& outBindingNames, std::vector<TypedSpan<uint32_t, VkShaderStageFlags>>& outBindingSpans) const
{	
	outBindingSpans.clear();

	std::vector<SpvReflectDescriptorBinding*> bindings;
	std::vector<VkShaderStageFlags>           bindingStageFlags;
	GatherSetBindings(shaderModuleNames, bindings, bindingStageFlags, outBindingSpans);

	outSetBindings.resize(bindings.size());
	outBindingNames.resize(bindings.size());
	for(size_t bindingIndex = 0; bindingIndex < outBindingSpans.size(); bindingIndex++)
	{
		SpvReflectDescriptorBinding* setBinding       = bindings[bindingIndex];
		VkShaderStageFlags           shaderStageFlags = bindingStageFlags[bindingIndex];

		uint32_t descriptorCount = setBinding->count;
		if(setBinding->type_description->op == SpvOpTypeRuntimeArray)
		{
			descriptorCount = (uint32_t)(-1); //A way to check for variable descriptor count
		}

		VkDescriptorSetLayoutBinding descriptorSetBinding =
		{
			.binding            = setBinding->binding,
			.descriptorType     = SpvToVkDescriptorType(setBinding->descriptor_type),
			.descriptorCount    = descriptorCount,
			.stageFlags         = shaderStageFlags,
			.pImmutableSamplers = nullptr
		};
		
		outSetBindings.push_back(descriptorSetBinding);
		outBindingNames.push_back(setBinding->name);
	}
}

VkShaderStageFlagBits Vulkan::ShaderDatabase::SpvToVkShaderStage(SpvReflectShaderStageFlagBits spvShaderStage) const
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

VkDescriptorType Vulkan::ShaderDatabase::SpvToVkDescriptorType(SpvReflectDescriptorType spvDescriptorType) const
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

void Vulkan::ShaderDatabase::GatherSetBindings(const std::span<std::wstring> shaderModuleNames, std::vector<SpvReflectDescriptorBinding*>& outBindings, std::vector<VkShaderStageFlags>& outBindingStageFlags, std::vector<TypedSpan<uint32_t, VkShaderStageFlags>>& outSetSpans) const
{
	uint32_t totalSetCount = 0;

	//Gather all bindings from all stages
	std::vector<SpvReflectDescriptorBinding*>                       allBindings;
	std::vector<TypedSpan<uint32_t, SpvReflectShaderStageFlagBits>> moduleBindingSpans;
	for(const std::wstring& shaderModuleName: shaderModuleNames)
	{
		const spv_reflect::ShaderModule& shaderModule = mLoadedShaderModules.at(shaderModuleName);

		uint32_t moduleBindingCount = 0;
		shaderModule.EnumerateDescriptorBindings(&moduleBindingCount, nullptr);

		TypedSpan<uint32_t, SpvReflectShaderStageFlagBits> moduleBindingSpan = 
		{
			.Begin = (uint32_t)allBindings.size(),
			.End   = (uint32_t)allBindings.size() + moduleBindingCount,
			.Type  = shaderModule.GetShaderStage()
		};

		moduleBindingSpans.push_back(moduleBindingSpan);
		allBindings.resize(allBindings.size() + moduleBindingCount);

		shaderModule.EnumerateDescriptorBindings(&moduleBindingCount, allBindings.data() + moduleBindingSpan.Begin);
		for(uint32_t bindingIndex = moduleBindingSpan.Begin; bindingIndex < moduleBindingSpan.End; bindingIndex++)
		{
			SpvReflectDescriptorBinding* binding = allBindings[bindingIndex];
			totalSetCount = std::max(totalSetCount, binding->set + 1);
		}
	}

	//Calculate the binding count for each set and store it in outSetSpans[binding->set].End
	outSetSpans.resize(totalSetCount, {.Begin = 0, .End = 0, .Type = 0});
	for(size_t spanIndex = 0; spanIndex < moduleBindingSpans.size(); spanIndex++)
	{
		TypedSpan<uint32_t, SpvReflectShaderStageFlagBits> bindingSpan = moduleBindingSpans[spanIndex];
		for(uint32_t bindingIndex = bindingSpan.Begin; bindingIndex < bindingSpan.End; bindingIndex++)
		{
			SpvReflectDescriptorBinding* binding = allBindings[bindingIndex];
			outSetSpans[binding->set].End   = std::max(outSetSpans[binding->set].End, binding->binding + 1);
			outSetSpans[binding->set].Type |= SpvToVkShaderStage(bindingSpan.Type);
		}
	}

	//Now outSetSpans[i].End contains the binding count for ith set. Accumulate the spans
	uint32_t accumulatedBindingCount = 0;
	for(TypedSpan<uint32_t, VkShaderStageFlags>& bindingSpan: outSetSpans)
	{
		uint32_t writtenBindingCount = bindingSpan.End;

		bindingSpan.Begin = accumulatedBindingCount;
		bindingSpan.End   = accumulatedBindingCount + writtenBindingCount;

		accumulatedBindingCount += writtenBindingCount;
	}

	//Fill out the bindings
	outBindings.resize(accumulatedBindingCount, nullptr);
	outBindingStageFlags.resize(accumulatedBindingCount, 0);
	for(TypedSpan<uint32_t, SpvReflectShaderStageFlagBits> moduleSpan: moduleBindingSpans)
	{
		const VkShaderStageFlags                      shaderStage       = SpvToVkShaderStage(moduleSpan.Type);
		const std::span<SpvReflectDescriptorBinding*> moduleBindingSpan = {allBindings.begin() + moduleSpan.Begin, allBindings.begin() + moduleSpan.End};

		for(SpvReflectDescriptorBinding* descriptorBinding: moduleBindingSpan)
		{
			TypedSpan<uint32_t, VkShaderStageFlags> setSpan          = outSetSpans[descriptorBinding->set];
			uint32_t                                flatBindingIndex = setSpan.Begin + descriptorBinding->binding;

			//Name should be defined in every shader for every uniform block
			assert(descriptorBinding->name != nullptr && descriptorBinding->name[0] != '\0');

			if(outBindings[flatBindingIndex] == nullptr)
			{
				outBindings[flatBindingIndex]          = descriptorBinding;
				outBindingStageFlags[flatBindingIndex] = shaderStage;
			}
			else
			{
				//Make sure the binding is the same
				SpvReflectDescriptorBinding* definedBinding = outBindings[flatBindingIndex];

				assert(strcmp(descriptorBinding->name, definedBinding->name) == 0);
				assert(descriptorBinding->resource_type == definedBinding->resource_type);
				assert(descriptorBinding->count         == definedBinding->count);

				outBindingStageFlags[flatBindingIndex] |= shaderStage;
			}
		}
	}
}