#include "VulkanShaders.hpp"
#include "VulkanUtils.hpp"
#include "VulkanFunctions.hpp"
#include "../../Core/Util.hpp"
#include "../../Logging/Logger.hpp"
#include "VulkanSharedDescriptorDatabaseBuilder.hpp"
#include "FrameGraph/VulkanPassDescriptorDatabaseBuilder.hpp"
#include <VulkanGenericStructures.h>
#include <cassert>
#include <unordered_map>
#include <array>
#include <algorithm>

Vulkan::ShaderDatabase::ShaderDatabase(LoggerQueue* logger): mLogger(logger)
{
}

Vulkan::ShaderDatabase::~ShaderDatabase()
{
}

void Vulkan::ShaderDatabase::RegisterShaderGroup(std::string_view groupName, std::span<std::wstring> shaderPaths, SharedDescriptorDatabaseBuilder* sharedDescriptorDatabaseBuilder, PassDescriptorDatabaseBuilder* passDescriptorDatabaseBuilder)
{
	assert(!mSetLayoutSpansPerShaderGroup.contains(groupName));

	for(const std::wstring& shaderPath: shaderPaths)
	{
		if(!mLoadedShaderModules.contains(shaderPath))
		{
			std::vector<uint32_t> shaderData;
			VulkanUtils::LoadShaderModuleFromFile(shaderPath, shaderData, mLogger);

			mLoadedShaderModules.emplace(std::make_pair(shaderPath, spv_reflect::ShaderModule(shaderData)));
		}
	}

	std::vector<VkDescriptorSetLayoutBinding> setBindings;
	std::vector<std::string>                  setBindingNames;
	FindBindings(shaderPaths, setBindings, setBindingNames);

	Span<uint32_t> layoutRecordSpan = 
	{
		.Begin = (uint32_t)mSetLayoutRecords.size(),
		.End   = (uint32_t)(mSetLayoutRecords.size() + setSpans.size())
	};

	for(TypedSpan<uint32_t, VkShaderStageFlags> setSpan: setSpans)
	{
		std::span<VkDescriptorSetLayoutBinding> bindingSpan = {setBindings.begin()     + setSpan.Begin, setBindings.begin()     + setSpan.End};
		std::span<std::string>                  nameSpan    = {setBindingNames.begin() + setSpan.Begin, setBindingNames.begin() + setSpan.End};

		uint16_t setDatabaseType   = 0xff; //Try shared set first
		uint16_t setRegisterResult = sharedDescriptorDatabaseBuilder->TryRegisterSet(bindingSpan, nameSpan);
		if(setRegisterResult == 0xff)
		{
			setRegisterResult = passDescriptorDatabaseBuilder->TryRegisterSet(bindingSpan, nameSpan);
			setDatabaseType   = passDescriptorDatabaseBuilder->GetPassTypeId();
		}

		mSetLayoutRecords.push_back(SetLayoutRecord
		{
			.Type = setDatabaseType,
			.Id   = setRegisterResult
		});
	}

	mSetLayoutSpansPerShaderGroup[groupName] = layoutRecordSpan;
}

void Vulkan::ShaderDatabase::GetRegisteredShaderInfo(const std::wstring& path, const uint32_t** outShaderData, uint32_t* outShaderSize) const
{
	assert(outShaderData != nullptr);
	assert(outShaderSize != nullptr);

	const spv_reflect::ShaderModule& shaderModule = mLoadedShaderModules.at(path);

	*outShaderData = shaderModule.GetCode();
	*outShaderSize = shaderModule.GetCodeSize();
}

void Vulkan::ShaderDatabase::GetPushConstantInfo(std::string_view groupName, std::string_view pushConstantName, uint32_t* outPushConstantOffset, VkShaderStageFlags* outShaderStages)
{
	Span<uint32_t> pushConstantSpan = mPushConstantSpansPerShaderGroup.at(groupName);

	const auto rangeBegin = mPushConstantRecords.begin() + pushConstantSpan.Begin;
	const auto rangeEnd   = mPushConstantRecords.begin() + pushConstantSpan.End;
	auto pushConstantRecord = std::find(rangeBegin, rangeEnd, [pushConstantName](const PushConstantRecord& rec)
	{
		return pushConstantName == rec.Name;
	});

	if(pushConstantRecord != rangeEnd)
	{
		if(outPushConstantOffset != nullptr)
		{
			*outPushConstantOffset = pushConstantRecord->Offset;
		}

		if(outShaderStages != nullptr)
		{
			*outShaderStages = pushConstantRecord->ShaderStages;
		}
	}
}

void Vulkan::ShaderDatabase::FindBindings(const std::span<std::wstring> shaderModuleNames, std::vector<VkDescriptorSetLayoutBinding>& outSetBindings, std::vector<std::string>& outBindingNames) const
{	
	outSetBindings.clear();
	outBindingNames.clear();

	std::vector<SpvReflectDescriptorBinding*> allBindings;
	CollectBindings(shaderModuleNames, allBindings);

	uint32_t totalSetCount = 0;
	for(SpvReflectDescriptorBinding* binding: allBindings)
	{
		totalSetCount = std::max(binding->set + 1, totalSetCount);
	}

	std::sort(allBindings.begin(), allBindings.end(), [](SpvReflectDescriptorBinding* left, SpvReflectDescriptorBinding* right)
	{
		return strcmp(left->name, right->name) < 0;
	});

	//Merge bindings by name
	std::vector<SpvReflectDescriptorBinding*> uniqueBindings;
	uniqueBindings.reserve(allBindings.size());

	const char* currBindingName = "";
	for(SpvReflectDescriptorBinding* binding: allBindings)
	{
		if(strcmp(binding->name, currBindingName) == 0)
		{
			#error "Not implemented"
		}
		else
		{
			#error "Not implemented"
		}
	}

	//Sort by <set, binding>
	std::sort(uniqueBindings.begin(), uniqueBindings.end(), [](SpvReflectDescriptorBinding* left, SpvReflectDescriptorBinding* right)
	{
		return left->binding < right->binding;
	});

	std::stable_sort(uniqueBindings.begin(), uniqueBindings.end(), [](SpvReflectDescriptorBinding* left, SpvReflectDescriptorBinding* right)
	{
		return left->set < right->set;
	});

	//Calculate the binding count for each set and store it in outSetSpans[binding->set].End
	outBindingSpans.resize(totalSetCount, {.Begin = 0, .End = 0, .Type = 0});
	for(size_t spanIndex = 0; spanIndex < moduleBindingSpans.size(); spanIndex++)
	{
		TypedSpan<uint32_t, SpvReflectShaderStageFlagBits> bindingSpan = moduleBindingSpans[spanIndex];
		for(uint32_t bindingIndex = bindingSpan.Begin; bindingIndex < bindingSpan.End; bindingIndex++)
		{
			SpvReflectDescriptorBinding* binding = allBindings[bindingIndex];
			outBindingSpans[binding->set].End   = std::max(outBindingSpans[binding->set].End, binding->binding + 1);
			outBindingSpans[binding->set].Type |= SpvToVkShaderStage(bindingSpan.Type);
		}
	}

	//Now outSetSpans[i].End contains the binding count for ith set. Accumulate the spans
	uint32_t accumulatedBindingCount = 0;
	for(TypedSpan<uint32_t, VkShaderStageFlags>& bindingSpan: outBindingSpans)
	{
		uint32_t writtenBindingCount = bindingSpan.End;

		bindingSpan.Begin = accumulatedBindingCount;
		bindingSpan.End   = accumulatedBindingCount + writtenBindingCount;

		accumulatedBindingCount += writtenBindingCount;
	}

	//Fill out the bindings
	outSetBindings.resize(accumulatedBindingCount);
	outBindingNames.resize(accumulatedBindingCount);
	for(TypedSpan<uint32_t, SpvReflectShaderStageFlagBits> moduleSpan: moduleBindingSpans)
	{
		const VkShaderStageFlags                      shaderStage       = SpvToVkShaderStage(moduleSpan.Type);
		const std::span<SpvReflectDescriptorBinding*> moduleBindingSpan = {allBindings.begin() + moduleSpan.Begin, allBindings.begin() + moduleSpan.End};

		for(SpvReflectDescriptorBinding* spvDescriptorBinding: moduleBindingSpan)
		{
			TypedSpan<uint32_t, VkShaderStageFlags> setSpan          = outBindingSpans[spvDescriptorBinding->set];
			uint32_t                                flatBindingIndex = setSpan.Begin + spvDescriptorBinding->binding;

			//Name should be defined in every shader for every uniform block
			assert(spvDescriptorBinding->name != nullptr && spvDescriptorBinding->name[0] != '\0');

			if(outSetBindings[flatBindingIndex].binding == (uint32_t)(-1))
			{
				outBindings[flatBindingIndex]          = spvDescriptorBinding;
				outBindingStageFlags[flatBindingIndex] = shaderStage;
			}
			else
			{
				//Make sure the binding is the same
				SpvReflectDescriptorBinding* definedBinding = outBindings[flatBindingIndex];

				assert(strcmp(spvDescriptorBinding->name, definedBinding->name) == 0);
				assert(spvDescriptorBinding->resource_type == definedBinding->resource_type);
				assert(spvDescriptorBinding->count         == definedBinding->count);

				outBindingStageFlags[flatBindingIndex] |= shaderStage;
			}

			VkShaderStageFlags shaderStageFlags = bindingStageFlags[bindingIndex];

			uint32_t descriptorCount = spvDescriptorBinding->count;
			if(spvDescriptorBinding->type_description->op == SpvOpTypeRuntimeArray)
			{
				descriptorCount = (uint32_t)(-1); //A way to check for variable descriptor count
			}

			VkDescriptorSetLayoutBinding vkDescriptorBinding =
			{
				.binding            = spvDescriptorBinding->binding,
				.descriptorType     = SpvToVkDescriptorType(spvDescriptorBinding->descriptor_type),
				.descriptorCount    = descriptorCount,
				.stageFlags         = shaderStageFlags,
				.pImmutableSamplers = nullptr
			};
		
			outSetBindings.push_back(vkDescriptorBinding);
			outBindingNames.push_back(setBinding->name);
		}
	}
}

void Vulkan::ShaderDatabase::CollectBindings(const std::span<std::wstring> shaderModuleNames, std::vector<SpvReflectDescriptorBinding*>& outSeparateBindings) const
{
	//Gather all bindings from all stages
	for(const std::wstring& shaderModuleName: shaderModuleNames)
	{
		const spv_reflect::ShaderModule& shaderModule = mLoadedShaderModules.at(shaderModuleName);

		uint32_t moduleBindingCount = 0;
		shaderModule.EnumerateDescriptorBindings(&moduleBindingCount, nullptr);

		size_t newBindingOffset = outSeparateBindings.size();
		outSeparateBindings.resize(outSeparateBindings.size() + moduleBindingCount);
		shaderModule.EnumerateDescriptorBindings(&moduleBindingCount, outSeparateBindings.data() + newBindingOffset);
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