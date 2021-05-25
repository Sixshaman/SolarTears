#pragma once

#include <vulkan/vulkan.h>

namespace Vulkan
{
	class ShaderManager;

	class DescriptorManager
	{
	public:
		DescriptorManager(const VkDevice device, const ShaderManager* shaderManager);
		~DescriptorManager();

		VkDescriptorSetLayout GetGBufferUniformsDescriptorSetLayout() const;
		VkDescriptorSetLayout GetGBufferTexturesDescriptorSetLayout() const;

	private:
		void CreateSamplers();
		void CreateDescriptorSetLayouts(const ShaderManager* shaderManager);

	private:
		const VkDevice mDeviceRef;

		VkDescriptorSetLayout mGBufferUniformsDescriptorSetLayout;
		VkDescriptorSetLayout mGBufferTexturesDescriptorSetLayout;

		VkSampler mLinearSampler;
	};
}