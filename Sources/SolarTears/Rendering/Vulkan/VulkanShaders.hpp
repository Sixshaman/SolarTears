#pragma once

#include "../../../3rdParty/SPIRV-Reflect/spirv_reflect.h"
#include <vulkan/vulkan.h>
#include <span> 
#include <array>
#include <unordered_set>
#include "../../Core/DataStructures/Span.hpp"

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

	public:
		ShaderManager(LoggerQueue* logger, VkDevice device);
		~ShaderManager();

		spv_reflect::ShaderModule LoadShaderBlob(const std::wstring& path) const;

		void FindBindings(const std::span<spv_reflect::ShaderModule*>& shaderModules, std::vector<VkDescriptorSetLayoutBinding>& outSetBindings, std::vector<std::string>& outBindingNames, std::vector<Span<uint32_t>>& outBindingSpans) const;

	private:
		void CreateSamplers();

		void GatherSetBindings(const std::span<spv_reflect::ShaderModule*>& shaderModules, std::vector<SpvReflectDescriptorBinding*>& outBindings, std::vector<VkShaderStageFlags>& outBindingStageFlags, std::vector<Span<uint32_t>>& outSetSpans) const;

		VkShaderStageFlagBits SpvToVkShaderStage(SpvReflectShaderStageFlagBits spvShaderStage)  const;
		VkDescriptorType      SpvToVkDescriptorType(SpvReflectDescriptorType spvDescriptorType) const;

	private:
		LoggerQueue* mLogger;

		const VkDevice mDeviceRef;

		std::array<VkSampler, (size_t)SamplerType::Count> mSamplers;
	};
}