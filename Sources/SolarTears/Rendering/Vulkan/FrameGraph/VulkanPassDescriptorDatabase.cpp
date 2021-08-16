#include "VulkanPassDescriptorDatabase.hpp"
#include "../VulkanUtils.hpp"
#include "../VulkanFunctions.hpp"
#include <algorithm>

Vulkan::PassDescriptorDatabase::PassDescriptorDatabase(const VkDevice device): mDeviceRef(device)
{
}

Vulkan::PassDescriptorDatabase::~PassDescriptorDatabase()
{
	for(VkDescriptorSetLayout& layoutToDestroy: mDescriptorSetLayouts)
	{
		SafeDestroyObject(vkDestroyDescriptorSetLayout, mDeviceRef, layoutToDestroy);
	}
}

void Vulkan::PassDescriptorDatabase::RegisterRequiredSet(std::string_view passName, std::span<VkDescriptorSetLayoutBinding> bindingSpan, std::span<std::string> bindingNameSpan)
{
	auto passIt = mSetBindingNamesPerPass.find(passName);
	if(passIt == mSetBindingNamesPerPass.end())
	{
		#error "Not implemented"
	}
	else
	{
		//Compare with one of the registered bindings
		pass->Validate
	}
}

void Vulkan::PassDescriptorDatabase::PostToDatabase(std::span<VkDescriptorSetLayout> descriptorSetLayouts, std::span<PassDatabaseRequest> bindingRequests)
{
	size_t firstAddedLayoutIndex = mDescriptorSetLayouts.size();
	mDescriptorSetLayouts.insert(mDescriptorSetLayouts.end(), descriptorSetLayouts.begin(), descriptorSetLayouts.end());

	auto newBindingsIt = mDescriptorBindings.end();
	mDescriptorBindings.resize(mDescriptorBindings.size() + bindingRequests.size());
	
	std::transform(bindingRequests.begin(), bindingRequests.end(), newBindingsIt, [firstAddedLayoutIndex](const PassDatabaseRequest& databaseRequest)
	{
		return PassDatabaseBinding
		{
			.SetLayoutIndex = (uint32_t)(firstAddedLayoutIndex + databaseRequest.SpanSetLayoutIndex),
			.Binding        = databaseRequest.Binding,
			.SubresourceId  = databaseRequest.SubresourceId
		};
	});
}