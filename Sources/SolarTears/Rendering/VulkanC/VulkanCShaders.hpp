#pragma once

#include "../../../3rdParty/SPIRV-Reflect/spirv_reflect.h"
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <memory>

class LoggerQueue;

namespace VulkanCBindings
{
	class ShaderManager
	{
		struct DescriptorBindingInfo
		{
			uint32_t           Binding;
			uint32_t           Set;
			uint32_t           Count;
			VkShaderStageFlags StageFlags;
			VkDescriptorType   DescriptorType;
		};

	public:
		ShaderManager(LoggerQueue* logger);
		~ShaderManager();

	public:
		const uint32_t* GetGBufferVertexShaderData()   const;
		const uint32_t* GetGBufferFragmentShaderData() const;

		size_t GetGBufferVertexShaderSize()   const;
		size_t GetGBufferFragmentShaderSize() const;

		void GetGBufferDrawDescriptorBindingInfo(const std::string& bindingName, uint32_t* outBinding, uint32_t* outSet, uint32_t* outCount, VkShaderStageFlags* outStageFlags, VkDescriptorType* outDescriptorType) const;

	private:
		void LoadShaderData();
		void FindDescriptorBindings();

		void GatherDescriptorBindings(const spv_reflect::ShaderModule* shaderModule, std::unordered_map<std::string, DescriptorBindingInfo>& descriptorBindingMap, VkShaderStageFlagBits shaderStage) const;

	private:
		LoggerQueue* mLogger;

		std::unordered_map<SpvReflectDescriptorType, VkDescriptorType> mSpvToVkDescriptorTypes;
		std::unordered_map<SpvReflectFormat,         VkFormat>         mSpvToVkFormats;

		std::unique_ptr<spv_reflect::ShaderModule> mGBufferVertexShaderModule;
		std::unique_ptr<spv_reflect::ShaderModule> mGBufferFragmentShaderModule;

		std::unordered_map<std::string, DescriptorBindingInfo> mGBufferDrawBindings;
	};
}