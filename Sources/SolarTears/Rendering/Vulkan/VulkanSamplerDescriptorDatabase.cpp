#include "VulkanSamplerDescriptorDatabase.hpp"
#include "VulkanUtils.hpp"
#include "VulkanFunctions.hpp"

Vulkan::SamplerDescriptorDatabase::SamplerDescriptorDatabase(const VkDevice device): mDeviceRef(device)
{
	mSamplerDescriptorSetLayout = VK_NULL_HANDLE;
	ResetRegisteredSets();
}

Vulkan::SamplerDescriptorDatabase::~SamplerDescriptorDatabase()
{
	SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, mSamplerDescriptorSetLayout);

	for(size_t i = 0; i < mSamplers.size(); i++)
	{
		SafeDestroyObject(vkDestroySampler, mDeviceRef, mSamplers[i]);
	}
}

void Vulkan::SamplerDescriptorDatabase::ResetRegisteredSets()
{
	mRegisteredSamplerShaderFlags = 0;
}

void Vulkan::SamplerDescriptorDatabase::RegisterSamplerSet(VkShaderStageFlags samplerShaderFlags)
{
	mRegisteredSamplerShaderFlags |= samplerShaderFlags;
}

void Vulkan::SamplerDescriptorDatabase::RecreateDescriptorSets()
{
	SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, mSamplerDescriptorSetLayout);

	VkDescriptorSetLayoutBinding samplerBinding;
	samplerBinding.binding            = 0;
	samplerBinding.descriptorCount    = (uint32_t)mSamplers.size();
	samplerBinding.descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER;
	samplerBinding.stageFlags         = mRegisteredSamplerShaderFlags;
	samplerBinding.pImmutableSamplers = mSamplers.data();

	std::array setLayoutBindings = {samplerBinding};

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
	descriptorSetLayoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext        = nullptr;
	descriptorSetLayoutCreateInfo.flags        = 0;
	descriptorSetLayoutCreateInfo.bindingCount = (uint32_t)setLayoutBindings.size();
	descriptorSetLayoutCreateInfo.pBindings    = setLayoutBindings.data();

	ThrowIfFailed(vkCreateDescriptorSetLayout(mDeviceRef, &descriptorSetLayoutCreateInfo, nullptr, &mSamplerDescriptorSetLayout));
}

bool Vulkan::SamplerDescriptorDatabase::IsSamplerDescriptorSet(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames)
{
	if(bindingNames.size() == 0)
	{
		return false;
	}

	if(bindingNames[0] == "Samplers")
	{
		assert(bindingNames.size() == 1);
		assert(setBindings.size() == 1);

		const VkDescriptorSetLayoutBinding& binding = setBindings[0];
		assert(binding.descriptorType  == VK_DESCRIPTOR_TYPE_SAMPLER);
		assert(binding.binding         == 0);
		assert(binding.descriptorCount == (uint32_t)(-1));
	}

	return false;
}

void Vulkan::SamplerDescriptorDatabase::CreateSamplers()
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