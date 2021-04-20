#include "VulkanShaders.hpp"
#include "VulkanUtils.hpp"
#include "../../Core/Util.hpp"
#include "../../Logging/Logger.hpp"
#include <cassert>
#include <array>

Vulkan::ShaderManager::ShaderManager(LoggerQueue* logger): mLogger(logger)
{
	mSpvToVkDescriptorTypes[SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER]                = VK_DESCRIPTOR_TYPE_SAMPLER;
	mSpvToVkDescriptorTypes[SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	mSpvToVkDescriptorTypes[SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE]          = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	mSpvToVkDescriptorTypes[SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE]          = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	mSpvToVkDescriptorTypes[SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER]   = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
	mSpvToVkDescriptorTypes[SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER]   = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
	mSpvToVkDescriptorTypes[SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER]         = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	mSpvToVkDescriptorTypes[SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER]         = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	mSpvToVkDescriptorTypes[SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	mSpvToVkDescriptorTypes[SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	mSpvToVkDescriptorTypes[SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT]       = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;

	mSpvToVkFormats[SPV_REFLECT_FORMAT_UNDEFINED]           = VK_FORMAT_UNDEFINED;
	mSpvToVkFormats[SPV_REFLECT_FORMAT_R32_UINT]            = VK_FORMAT_R32_UINT;
	mSpvToVkFormats[SPV_REFLECT_FORMAT_R32_SINT]            = VK_FORMAT_R32_SINT;
	mSpvToVkFormats[SPV_REFLECT_FORMAT_R32_SFLOAT]          = VK_FORMAT_R32_SFLOAT;
	mSpvToVkFormats[SPV_REFLECT_FORMAT_R32G32_UINT]         = VK_FORMAT_R32G32_UINT;
	mSpvToVkFormats[SPV_REFLECT_FORMAT_R32G32_SINT]         = VK_FORMAT_R32G32_SINT;
	mSpvToVkFormats[SPV_REFLECT_FORMAT_R32G32_SFLOAT]       = VK_FORMAT_R32G32_SFLOAT;
	mSpvToVkFormats[SPV_REFLECT_FORMAT_R32G32B32_UINT]      = VK_FORMAT_R32G32B32_UINT;
	mSpvToVkFormats[SPV_REFLECT_FORMAT_R32G32B32_SINT]      = VK_FORMAT_R32G32B32_SINT;
	mSpvToVkFormats[SPV_REFLECT_FORMAT_R32G32B32_SFLOAT]    = VK_FORMAT_R32G32B32_SFLOAT;
	mSpvToVkFormats[SPV_REFLECT_FORMAT_R32G32B32A32_UINT]   = VK_FORMAT_R32G32B32A32_UINT;
	mSpvToVkFormats[SPV_REFLECT_FORMAT_R32G32B32A32_SINT]   = VK_FORMAT_R32G32B32A32_SINT;
	mSpvToVkFormats[SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT] = VK_FORMAT_R32G32B32A32_SFLOAT;

	LoadShaderData();
	FindDescriptorBindings();
}

Vulkan::ShaderManager::~ShaderManager()
{
}

const uint32_t* Vulkan::ShaderManager::GetGBufferVertexShaderData() const
{
	return mGBufferVertexShaderModule->GetCode();
}

const uint32_t* Vulkan::ShaderManager::GetGBufferFragmentShaderData() const
{
	return mGBufferFragmentShaderModule->GetCode();
}

size_t Vulkan::ShaderManager::GetGBufferVertexShaderSize() const
{
	return mGBufferVertexShaderModule->GetCodeSize();
}

size_t Vulkan::ShaderManager::GetGBufferFragmentShaderSize() const
{
	return mGBufferFragmentShaderModule->GetCodeSize();
}

void Vulkan::ShaderManager::GetGBufferDrawDescriptorBindingInfo(const std::string& bindingName, uint32_t* outBinding, uint32_t* outSet, uint32_t* outCount, VkShaderStageFlags* outStageFlags, VkDescriptorType* outDescriptorType) const
{
	DescriptorBindingInfo bindingInfo = mGBufferDrawBindings.at(bindingName);

	if(outBinding != nullptr)
	{
		*outBinding = bindingInfo.Binding;
	}

	if(outSet != nullptr)
	{
		*outSet = bindingInfo.Set;
	}

	if(outCount != nullptr)
	{
		*outCount = bindingInfo.Count;
	}

	if(outStageFlags != nullptr)
	{
		*outStageFlags = bindingInfo.StageFlags;
	}

	if(outDescriptorType != nullptr)
	{
		*outDescriptorType = bindingInfo.DescriptorType;
	}
}

void Vulkan::ShaderManager::LoadShaderData()
{
	//Shader states
	const std::wstring gbufferShaderDir = Utils::GetMainDirectory() + L"Shaders/Vulkan/GBuffer/";

	std::vector<uint32_t> vertexShaderData;
	VulkanUtils::LoadShaderModuleFromFile(gbufferShaderDir + L"GBufferDraw.vert.spv", vertexShaderData, mLogger);

	std::vector<uint32_t> fragmentShaderData;
	VulkanUtils::LoadShaderModuleFromFile(gbufferShaderDir + L"GBufferDraw.frag.spv", fragmentShaderData, mLogger);

	mGBufferVertexShaderModule   = std::make_unique<spv_reflect::ShaderModule>(vertexShaderData);
	mGBufferFragmentShaderModule = std::make_unique<spv_reflect::ShaderModule>(fragmentShaderData);

	assert(mGBufferVertexShaderModule->GetResult()   == SPV_REFLECT_RESULT_SUCCESS);
	assert(mGBufferFragmentShaderModule->GetResult() == SPV_REFLECT_RESULT_SUCCESS);
}

void Vulkan::ShaderManager::FindDescriptorBindings()
{
	GatherDescriptorBindings(mGBufferVertexShaderModule.get(),   mGBufferDrawBindings, VK_SHADER_STAGE_VERTEX_BIT);
	GatherDescriptorBindings(mGBufferFragmentShaderModule.get(), mGBufferDrawBindings, VK_SHADER_STAGE_FRAGMENT_BIT);
}

void Vulkan::ShaderManager::GatherDescriptorBindings(const spv_reflect::ShaderModule* shaderModule, std::unordered_map<std::string, DescriptorBindingInfo>& descriptorBindingMap, VkShaderStageFlagBits shaderStage) const
{
	uint32_t descriptorBindingCount = 0;
	shaderModule->EnumerateDescriptorBindings(&descriptorBindingCount, nullptr);

	std::vector<SpvReflectDescriptorBinding*> descriptorBindings(descriptorBindingCount);
	shaderModule->EnumerateDescriptorBindings(&descriptorBindingCount, descriptorBindings.data());

	for(SpvReflectDescriptorBinding* reflectBinding: descriptorBindings)
	{
		auto bindIt = descriptorBindingMap.find(reflectBinding->name);

		if(bindIt == descriptorBindingMap.end())
		{
			DescriptorBindingInfo bindingInfo;
			bindingInfo.Binding        = reflectBinding->binding;
			bindingInfo.Set            = reflectBinding->set;
			bindingInfo.DescriptorType = mSpvToVkDescriptorTypes.at(reflectBinding->descriptor_type);
			bindingInfo.Count          = reflectBinding->count;
			bindingInfo.StageFlags     = shaderStage;

			std::string descriptorName;
			if(reflectBinding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER || reflectBinding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
			{
				descriptorName = reflectBinding->type_description->type_name;
			}
			else
			{
				descriptorName = reflectBinding->name;
			}

			assert(descriptorName.size() != 0);

			descriptorBindingMap[descriptorName] = bindingInfo;
		}
		else
		{
			assert(bindIt->second.Set == reflectBinding->set);

			assert(bindIt->second.Binding         == reflectBinding->binding);
			assert(bindIt->second.DescriptorType  == mSpvToVkDescriptorTypes.at(reflectBinding->descriptor_type));
			assert(bindIt->second.Count           == reflectBinding->count);

			bindIt->second.StageFlags |= shaderStage;
		}
	}
}