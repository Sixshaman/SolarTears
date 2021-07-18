#include "VulkanDescriptorManager.hpp"
#include "VulkanFunctions.hpp"
#include "VulkanShaders.hpp"
#include "VulkanUtils.hpp"
#include <array>

Vulkan::DescriptorManager::DescriptorManager(const VkDevice device, const ShaderManager* shaderManager): mDeviceRef(device)
{
	mGBufferUniformsDescriptorSetLayout = VK_NULL_HANDLE;
	mGBufferTexturesDescriptorSetLayout = VK_NULL_HANDLE;


	CreateDescriptorSetLayouts(shaderManager);
}

Vulkan::DescriptorManager::~DescriptorManager()
{
	SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, mGBufferUniformsDescriptorSetLayout);
	SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, mGBufferTexturesDescriptorSetLayout);

	SafeDestroyObject(vkDestroySampler, mDeviceRef, mLinearSampler);
}

VkDescriptorSetLayout Vulkan::DescriptorManager::GetGBufferUniformsDescriptorSetLayout() const
{
	return mGBufferUniformsDescriptorSetLayout;
}

VkDescriptorSetLayout Vulkan::DescriptorManager::GetGBufferTexturesDescriptorSetLayout() const
{
	return mGBufferTexturesDescriptorSetLayout;
}

void Vulkan::DescriptorManager::CreateSamplers()
{
	
}

void Vulkan::DescriptorManager::CreateDescriptorSetLayouts(const ShaderManager* shaderManager)
{
	std::array gbufferImmutableSamplers = {mLinearSampler};

	std::array gbufferUniformBindingNames = {"UniformBufferObject", "UniformBufferFrame"};
	std::array gbufferTextureBindingNames = {"ObjectTexture"};

	std::vector<VkDescriptorSetLayoutBinding> gbufferUniformsDescriptorSetBindings;
	for(size_t i = 0; i < gbufferUniformBindingNames.size(); i++)
	{
		uint32_t           bindingNumber   = (uint32_t)(-1);
		uint32_t           bindingSet      = (uint32_t)(-1);
		uint32_t           descriptorCount = (uint32_t)(-1);
		VkShaderStageFlags stageFlags      = 0;
		shaderManager->GetGBufferDrawDescriptorBindingInfo(gbufferUniformBindingNames[i], &bindingNumber, &bindingSet, &descriptorCount, &stageFlags, nullptr);

		assert(bindingSet == 1);

		VkDescriptorSetLayoutBinding descriptorSetLayoutBinding;
		descriptorSetLayoutBinding.binding            = bindingNumber;
		descriptorSetLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC; //Only dynamic uniform buffers are used
		descriptorSetLayoutBinding.descriptorCount    = descriptorCount;
		descriptorSetLayoutBinding.stageFlags         = stageFlags;
		descriptorSetLayoutBinding.pImmutableSamplers = nullptr;

		gbufferUniformsDescriptorSetBindings.push_back(descriptorSetLayoutBinding);
	}

	std::vector<VkDescriptorSetLayoutBinding> gbufferTexturesDescriptorSetBindings;
	for(size_t i = 0; i < gbufferTextureBindingNames.size(); i++)
	{
		uint32_t           bindingNumber   = (uint32_t)(-1);
		uint32_t           bindingSet      = (uint32_t)(-1);
		uint32_t           descriptorCount = (uint32_t)(-1);
		VkShaderStageFlags stageFlags      = 0;
		VkDescriptorType   descriptorType  = VK_DESCRIPTOR_TYPE_MAX_ENUM;
		shaderManager->GetGBufferDrawDescriptorBindingInfo(gbufferTextureBindingNames[i], &bindingNumber, &bindingSet, &descriptorCount, &stageFlags, &descriptorType);

		assert(bindingSet == 0);

		VkDescriptorSetLayoutBinding descriptorSetLayoutBinding;
		descriptorSetLayoutBinding.binding            = bindingNumber;
		descriptorSetLayoutBinding.descriptorType     = descriptorType;
		descriptorSetLayoutBinding.descriptorCount    = descriptorCount;
		descriptorSetLayoutBinding.stageFlags         = stageFlags;
		descriptorSetLayoutBinding.pImmutableSamplers = gbufferImmutableSamplers.data(); //Will be ignored if the descriptor type isn't a sampler

		gbufferTexturesDescriptorSetBindings.push_back(descriptorSetLayoutBinding);
	}

	VkDescriptorSetLayoutCreateInfo gbufferUniformsDescriptorSetLayoutCreateInfo;
	gbufferUniformsDescriptorSetLayoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	gbufferUniformsDescriptorSetLayoutCreateInfo.pNext        = nullptr;
	gbufferUniformsDescriptorSetLayoutCreateInfo.flags        = 0;
	gbufferUniformsDescriptorSetLayoutCreateInfo.bindingCount = (uint32_t)(gbufferUniformsDescriptorSetBindings.size());
	gbufferUniformsDescriptorSetLayoutCreateInfo.pBindings    = gbufferUniformsDescriptorSetBindings.data();

	ThrowIfFailed(vkCreateDescriptorSetLayout(mDeviceRef, &gbufferUniformsDescriptorSetLayoutCreateInfo, nullptr, &mGBufferUniformsDescriptorSetLayout));

	VkDescriptorSetLayoutCreateInfo gbufferTexturesDescriptorSetLayoutCreateInfo;
	gbufferTexturesDescriptorSetLayoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	gbufferTexturesDescriptorSetLayoutCreateInfo.pNext        = nullptr;
	gbufferTexturesDescriptorSetLayoutCreateInfo.flags        = 0;
	gbufferTexturesDescriptorSetLayoutCreateInfo.bindingCount = (uint32_t)(gbufferTexturesDescriptorSetBindings.size());
	gbufferTexturesDescriptorSetLayoutCreateInfo.pBindings    = gbufferTexturesDescriptorSetBindings.data();

	ThrowIfFailed(vkCreateDescriptorSetLayout(mDeviceRef, &gbufferTexturesDescriptorSetLayoutCreateInfo, nullptr, &mGBufferTexturesDescriptorSetLayout));
}