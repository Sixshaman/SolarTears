#pragma once

#include "../../../3rdParty/SPIRV-Reflect/spirv_reflect.h"
#include <vulkan/vulkan.h>
#include <span> 

class LoggerQueue;

namespace Vulkan
{
	class ShaderManager
	{
	public:
		ShaderManager(LoggerQueue* logger, VkDevice device);
		~ShaderManager();

		spv_reflect::ShaderModule LoadShaderBlob(const std::wstring& path) const;

		void CreatePipelineLayout(VkDevice device, const std::span<spv_reflect::ShaderModule*>& shaderModules, VkPipelineLayout* outPipelineLayout) const;

	private:
		void CreateSamplers();

		uint32_t GetModulesSetCount(const std::span<spv_reflect::ShaderModule*>& shaderModules) const;
		void GatherSetBindings(const std::span<spv_reflect::ShaderModule*>& shaderModules, std::vector<std::vector<SpvReflectDescriptorBinding*>>& outBindingsForSets, std::vector<std::vector<VkShaderStageFlags>>& outBindingsStageFlags) const;
		void BuildDescriptorBindingInfos(const std::span<SpvReflectDescriptorBinding*> spvSetBindings, const std::span<VkShaderStageFlags> setBindingStageFlags, const std::span<VkSampler> immutableSamplers, std::vector<VkDescriptorSetLayoutBinding>& outSetBindings, std::vector<VkDescriptorBindingFlags>& outSetFlags) const;

		VkShaderStageFlagBits SpvToVkShaderStage(SpvReflectShaderStageFlagBits spvShaderStage)  const;
		VkDescriptorType      SpvToVkDescriptorType(SpvReflectDescriptorType spvDescriptorType) const;

	private:
		LoggerQueue* mLogger;

		const VkDevice mDeviceRef;

		VkSampler mLinearSampler;
		VkSampler mAnisotropicSampler;
	};
}