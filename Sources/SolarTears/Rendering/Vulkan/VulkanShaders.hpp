#pragma once

#include "../../../3rdParty/SPIRV-Reflect/spirv_reflect.h"
#include <vulkan/vulkan.h>
#include <span> 
#include <unordered_map>
#include "../../Core/DataStructures/Span.hpp"

class LoggerQueue;

namespace Vulkan
{
	//Shader database for render passes.
	//We can't load shaders individually for each pass when needed because of a complex problem.
	//Suppose pass A accesses scene textures in vertex shader and pass B accesses them in fragment shader.
	//This means the texture descriptor set layout must have VERTEX | FRAGMENT shader flags.
	//Since we create descriptor set layouts based on reflection data, we have to:
	//1) Load all possible shaders for passes;
	//2) Go through the whole reflection data for all shaders and build descriptor set layouts;
	//3) Create passes only after that, giving them descriptor set layouts.
	//This is all to avoid loading the shaders twice - once to obtain the reflection data, once to create pipelines

	enum class ShaderGroupRegisterFlags: uint32_t
	{
		None         = 0x00,
		DontNeedSets = 0x01 //Don't create new descriptor set layouts for this shader group
	};

	class ShaderDatabase
	{
	public:
		ShaderDatabase(LoggerQueue* logger);
		~ShaderDatabase();

		void RegisterShaderGroup(const std::string& passName, const std::string& groupName, std::span<std::wstring> shaderPaths, ShaderGroupRegisterFlags groupRegisterFlags);

		void GetRegisteredShaderInfo(const std::wstring& path, const uint32_t** outShaderData, uint32_t* outShaderSize) const;

	private:
		void FindBindings(const std::span<std::wstring> shaderModuleNames, std::vector<VkDescriptorSetLayoutBinding>& outSetBindings, std::vector<std::string>& outBindingNames, std::vector<TypedSpan<uint32_t, VkShaderStageFlags>>& outBindingSpans) const;
		void GatherSetBindings(const std::span<std::wstring> shaderModuleNames, std::vector<SpvReflectDescriptorBinding*>& outBindings, std::vector<VkShaderStageFlags>& outBindingStageFlags, std::vector<TypedSpan<uint32_t, VkShaderStageFlags>>& outSetSpans) const;

		VkShaderStageFlagBits SpvToVkShaderStage(SpvReflectShaderStageFlagBits spvShaderStage)  const;
		VkDescriptorType      SpvToVkDescriptorType(SpvReflectDescriptorType spvDescriptorType) const;

	private:
		LoggerQueue* mLogger;

		std::vector<spv_reflect::ShaderModule>     mLoadedShaderModules;
		std::unordered_map<std::wstring, uint32_t> mShaderModuleIndices;
	};
}