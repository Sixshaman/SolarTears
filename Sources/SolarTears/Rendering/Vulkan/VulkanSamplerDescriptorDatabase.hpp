#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <span>
#include <string>

namespace Vulkan
{
	class SamplerDescriptorDatabase
	{
		enum class SamplerType : uint32_t
		{
			Linear = 0,
			Anisotropic,

			Count
		};

	public:
		SamplerDescriptorDatabase(const VkDevice device);
		~SamplerDescriptorDatabase();

		bool IsSamplerDescriptorSet(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string> bindingNames);

		void ResetRegisteredSets();
		void RegisterSamplerSet(VkShaderStageFlags samplerShaderFlags);

		void RecreateDescriptorSets();

	private:
		void CreateSamplers();

	private:
		const VkDevice mDeviceRef;

		VkDescriptorSetLayout mSamplerDescriptorSetLayout;
		VkShaderStageFlags    mRegisteredSamplerShaderFlags;

		std::array<VkSampler, (size_t)SamplerType::Count> mSamplers;
	};
}