#pragma once

#include "../../../3rdParty/SPIRV-Reflect/spirv_reflect.h"
#include <vulkan/vulkan.h>
#include <span> 
#include <unordered_map>
#include "../../Core/DataStructures/Span.hpp"
#include "FrameGraph/VulkanRenderPass.hpp"
#include "VulkanSharedDescriptorDatabase.hpp"

class LoggerQueue;

namespace Vulkan
{
	//Shader database for render passes.
	//We can't load shaders individually for each pass when needed because of a complex problem.
	//Suppose pass A accesses scene textures in vertex shader and pass B accesses them in fragment shader.
	//This means the (common) texture descriptor set layout must have VERTEX | FRAGMENT shader flags.
	//Since we create descriptor set layouts based on reflection data, we have to:
	//1) Load all possible shaders for passes;
	//2) Go through the whole reflection data for all shaders and build descriptor set layouts;
	//3) Create passes only after that, giving them descriptor set layouts.
	//This is all to avoid loading the shaders twice - once to obtain the reflection data, once to create pipelines

	class SamplerManager;

	class ShaderDatabase
	{
		static constexpr uint16_t SharedSetType = 0xff;

		//The data structure needed to store information about which passes use which set layouts
		struct SetLayoutRecord
		{
			uint16_t Type; //Type of the pass that uses the set (Shared, Pass 1, Pass 2, etc.)
			uint16_t Id;   //Per-type set layout id
		};

		//The data structure to store information about pass constants used by a shader group
		struct PushConstantRecord
		{
			std::string_view   Name;
			uint32_t           Offset;
			VkShaderStageFlags ShaderStages;
		};

	public:
		ShaderDatabase(VkDevice device, SamplerManager* samplerManager, LoggerQueue* logger);
		~ShaderDatabase();

		void RegisterShaderGroup(std::string_view groupName, std::span<std::wstring> shaderPaths);

		void GetRegisteredShaderInfo(const std::wstring& path, const uint32_t** outShaderData, uint32_t* outShaderSize) const;

		void GetPushConstantInfo(std::string_view groupName, std::string_view pushConstantName, uint32_t* outPushConstantOffset, VkShaderStageFlags* outShaderStages);

		void FlushSharedSetLayoutInfos(SharedDescriptorDatabase* databaseToBuild);

	private:
		//Functions for collecting bindings and push constants
		void CollectBindings(const std::span<std::wstring> shaderModuleNames);
		void CollectPushConstants(const std::span<std::wstring> shaderModuleNames);

		//Divides inoutSpvSets into two parts: already known sets and new sets
		void SplitNewAndExistingSets(uint32_t existingSetCount, std::vector<SpvReflectDescriptorSet*>& inoutSpvSets, std::span<SpvReflectDescriptorSet*>* outExistingSetSpan, std::span<SpvReflectDescriptorSet*>* outNewSetSpan);

		//Finds the necessary set sizes for each of moduleUpdatedSets
		void CalculateUpdatedSetSizes(const std::span<SpvReflectDescriptorSet*> moduleUpdatedSets, const std::vector<Span<uint32_t>>& existingBindingSpansPerSet, std::vector<uint32_t>& outUpdatedSetSizes);

		//Updates inoutBindings with new set data
		void UpdateExistingSets(const std::span<SpvReflectDescriptorSet*> setUpdates, std::vector<SpvReflectDescriptorBinding*>& inoutBindings, std::vector<Span<uint32_t>>& inoutSetSpans);
		void AddNewSets(const std::span<SpvReflectDescriptorSet*> newSets, std::vector<SpvReflectDescriptorBinding*>& inoutBindings, std::vector<Span<uint32_t>>& inoutSetSpans);

		//Transform SPIR-V Reflect variables to Vulkan variables
		VkShaderStageFlagBits SpvToVkShaderStage(SpvReflectShaderStageFlagBits spvShaderStage)  const;
		VkDescriptorType      SpvToVkDescriptorType(SpvReflectDescriptorType spvDescriptorType) const;

	private:
		SetLayoutRecord AddSet(std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<std::string_view> bindingNames);

		void BuildSetLayouts();

		bool ValidateNewBinding(const VkDescriptorSetLayoutBinding& bindingInfo, SharedDescriptorDatabase::SharedDescriptorBindingType bindingType) const;
		bool ValidateExistingBinding(const VkDescriptorSetLayoutBinding& newBindingInfo, const VkDescriptorSetLayoutBinding& existingBindingInfo) const;

	private:
		LoggerQueue* mLogger;

		const VkDevice        mDeviceRef;
		const SamplerManager* mSamplerManagerRef;

		std::unordered_map<std::wstring, spv_reflect::ShaderModule> mLoadedShaderModules;

		//Set layout records for each shader group
		std::vector<SetLayoutRecord>                         mSetLayoutRecords;
		std::unordered_map<std::string_view, Span<uint32_t>> mSetLayoutSpansPerShaderGroup;

		//Push constants for each shader group
		//Each push constant span is lexicographically sorted by name
		std::vector<PushConstantRecord>                      mPushConstantRecords;
		std::unordered_map<std::string_view, Span<uint32_t>> mPushConstantSpansPerShaderGroup;


		std::vector<Span<uint32_t>>                                        mSharedSetBindingSpansPerLayout;
		std::vector<VkDescriptorSetLayoutBinding>                          mSharedSetLayoutBindingsFlat;
		std::vector<SharedDescriptorDatabase::SharedDescriptorBindingType> mSharedSetLayoutBindingTypesFlat;

		std::vector<VkDescriptorSetLayout> mSharedSetLayouts;
		std::unordered_map<std::string_view, SharedDescriptorDatabase::SharedDescriptorBindingType> mSharedBindingTypes;
	};
}