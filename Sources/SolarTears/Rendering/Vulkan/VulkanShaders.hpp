#pragma once

#include "../../../3rdParty/SPIRV-Reflect/spirv_reflect.h"
#include <vulkan/vulkan.h>
#include <span> 
#include <array>

class LoggerQueue;

namespace Vulkan
{
	class ShaderManager
	{
		enum class SamplerType: uint32_t
		{
			Linear = 0,
			Anisotropic,

			Count
		};

		static constexpr size_t ImmutableSamplerCount = 2;

	public:
		ShaderManager(LoggerQueue* logger, VkDevice device);
		~ShaderManager();

		spv_reflect::ShaderModule LoadShaderBlob(const std::wstring& path) const;

		void FindBindings(const std::span<spv_reflect::ShaderModule*>& shaderModules) const;

	private:
		void CreateSamplers();

		uint32_t GetModulesSetCount(const std::span<spv_reflect::ShaderModule*>& shaderModules) const;
		void GatherSetBindings(const std::span<spv_reflect::ShaderModule*>& shaderModules, std::vector<std::vector<SpvReflectDescriptorBinding*>>& outBindingsForSets, std::vector<std::vector<VkShaderStageFlags>>& outBindingsStageFlags) const;
		void BuildDescriptorBindingInfos(const std::span<SpvReflectDescriptorBinding*> spvSetBindings, const std::span<VkShaderStageFlags> setBindingStageFlags, std::vector<VkDescriptorSetLayoutBinding>& outSetBindings, std::vector<VkDescriptorBindingFlags>& outSetFlags) const;

		VkShaderStageFlagBits SpvToVkShaderStage(SpvReflectShaderStageFlagBits spvShaderStage)  const;
		VkDescriptorType      SpvToVkDescriptorType(SpvReflectDescriptorType spvDescriptorType) const;

	private:
		LoggerQueue* mLogger;

		const VkDevice mDeviceRef;

		std::array<VkSampler, (size_t)SamplerType::Count> mSamplers;
	};
}