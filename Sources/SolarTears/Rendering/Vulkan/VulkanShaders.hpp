#pragma once

#include "../../../3rdParty/SPIRV-Reflect/spirv_reflect.h"
#include <vulkan/vulkan.h>
#include <span> 
#include <unordered_map>
#include "../../Core/DataStructures/Span.hpp"
#include "../Common/FrameGraph/ModernFrameGraphMisc.hpp"
#include "FrameGraph/VulkanRenderPass.hpp"
#include "FrameGraph/VulkanSharedDescriptorDatabase.hpp"

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

	struct PassSetInfo
	{
		RenderPassType        PassType;
		VkDescriptorSetLayout SetLayout;
		Span<uint32_t>        BindingSpan;
	};

	class ShaderDatabase
	{
		using BindingDomain = uint16_t;
		using BindingType   = uint16_t; 

		static constexpr BindingDomain UndefinedSetDomain = 0xffff;
		static constexpr BindingDomain SharedSetDomain    = 0x0000;

		//The data structure to store information about the binding domain (Shared, Pass 1, Pass 2, etc)
		struct DomainRecord
		{
			RenderPassType PassType;
			uint32_t       FirstLayoutNodeIndex;
		};

		//The data structure to store information about which passes use which bindings
		struct BindingRecord
		{
			uint16_t Domain; //Type of the pass that uses the set (Shared, Pass 1, Pass 2, etc.)
			uint16_t Type;   //Per-domain binding type

			//VkDescriptorType DescriptorType;  //Expected descriptor type for the binding (for validation purposes)
			//uint32_t         DescriptorFlags; //Expected descriptor flags for the binding (for validation purposes)
		};

		//The node of a set layout information for a particular domain
		//All layouts for a domain are stored in an array-backed linked list
		struct SetLayoutRecordNode
		{
			VkDescriptorSetLayout SetLayout;     //Set layout object
			Span<uint32_t>        BindingSpan;   //Binding span for the set
			uint32_t              NextNodeIndex; //The index of the next binding span node in the same domain
		};

		//The data structure to store information about pass constants used by a shader group
		struct PushConstantRecord
		{
			std::string_view   Name;
			uint32_t           Offset;
			VkShaderStageFlags ShaderStages;
		};

		//The data structure to refer to ranges of push constant infos
		struct PushConstantSpans
		{
			Span<uint32_t> RecordSpan; //Span of PushConstantRecords
			Span<uint32_t> RangeSpan;  //Span of VkPushConstantRanges
		};

	public:
		ShaderDatabase(VkDevice device, SamplerManager* samplerManager, LoggerQueue* logger);
		~ShaderDatabase();

		//Registers a render pass in the database
		void RegisterPass(RenderPassType passType);
		void RegisterPassBinding(RenderPassType passType, std::string_view bindingName, uint16_t passBindingType);
		void SetPassBindingValidateInfo(VkDescriptorType descriptorType);
		void SetPassBindingFlags(std::string_view bindingName, VkDescriptorBindingFlags descriptorFlags);

		//Registers a shader group in the database
		void RegisterShaderGroup(std::string_view groupName, std::span<std::wstring> shaderPaths);

		//Get the byte-code and byte-code size of the registered shader in path
		void GetRegisteredShaderInfo(const std::wstring& path, const uint32_t** outShaderData, uint32_t* outShaderSize) const;

		//Get the offset + shader stages for the registered push constant pushConstantName in shader group groupName
		void GetPushConstantInfo(std::string_view groupName, std::string_view pushConstantName, uint32_t* outPushConstantOffset, VkShaderStageFlags* outShaderStages) const;

		//Creates a pipeline layout compatible for multiple shader groups
		//The set layouts in the pipeline layout are minimal matching set in groupNamesForSets
		//The push constants are minimal matching set in groupNamesForPushConstants
		//Returns nullptr if no matching pipeline layout can be created
		void CreateMatchingPipelineLayout(std::span<std::string_view> groupNamesForSets, std::span<std::string_view> groupNamesForPushConstants, VkPipelineLayout* outPipelineLayout) const;

		//Transfers the ownership of all set layouts from shared domain to shared descriptor database, also saving the information about layouts
		void FlushSharedSetLayouts(SharedDescriptorDatabase* databaseToBuild);

		void BuildUniquePassSetInfos(std::vector<PassSetInfo>& outPassSetInfos, std::vector<uint16_t>& outPassBindingTypes) const;

	private:
		//Functions for collecting bindings and push constants
		void RegisterBindings(const std::string_view groupName, const std::span<std::wstring> shaderModuleNames);
		void RegisterPushConstants(std::string_view groupName, const std::span<std::wstring> shaderModuleNames);

	private:
		//Divides inoutSpvSets into two parts: already known sets (with .set < existingSetCount) and new sets (with .set >= existingSetCount)
		void SplitSetsByPivot(uint32_t pivotSetIndex, std::vector<SpvReflectDescriptorSet*>& inoutSpvSets, std::span<SpvReflectDescriptorSet*>* outExistingSetSpan, std::span<SpvReflectDescriptorSet*>* outNewSetSpan);

		//Finds the necessary set sizes for each of moduleUpdatedSets
		void CalculateUpdatedSetSizes(const std::span<SpvReflectDescriptorSet*> moduleUpdatedSets, const std::vector<Span<uint32_t>>& existingBindingSpansPerSet, std::vector<uint32_t>& outUpdatedSetSizes);

		//Updates inoutBindings with new set data
		void MergeExistingSetBindings(const std::span<SpvReflectDescriptorSet*> setUpdates, std::vector<SpvReflectDescriptorBinding*>& inoutBindings, std::vector<Span<uint32_t>>& inoutSetSpans);
		void MergeNewSetBindings(const std::span<SpvReflectDescriptorSet*> newSets, std::vector<SpvReflectDescriptorBinding*>& inoutBindings, std::vector<Span<uint32_t>>& inoutSetSpans);

	private:
		//Add set layout info to the database
		uint32_t RegisterSetLayout(uint16_t setDomain, std::span<VkDescriptorSetLayoutBinding> setBindings, std::span<BindingRecord> setBindingRecords);

		//Detect set domain for binding name list
		uint16_t DetectSetDomain(std::span<std::string_view> bindingNames, std::span<BindingRecord> outSetBindingRecords);

		//Create set layouts from the database infos
		void BuildSetLayouts();

	private:
		//Collects all push constant records from shader modules
		void CollectPushConstantRecords(const std::span<std::wstring> shaderModuleNames, std::vector<PushConstantRecord>& outPushConstantRecords);

		//Registers push constant records in the database
		//The records are expected to be lexicographically sorted
		//Returns the span of the new records
		Span<uint32_t> RegisterPushConstantRecords(const std::span<PushConstantRecord> lexicographicallySortedRecords);

		//Registers push constant ranges in the database, created from provided push constant records
		//Returns the span of the new ranges
		Span<uint32_t> RegisterPushConstantRanges(const std::span<PushConstantRecord> records);

	private:
		//Finds a span in mSetLayoutsFlat
		Span<uint32_t> FindMatchingSetLayoutSpan(std::span<std::string_view> groupNames) const;

		//Finds a span in mPushConstantRanges
		Span<uint32_t> FindMatchingPushConstantRangeSpan(std::span<std::string_view> groupNames) const;

	private:
		//Transform SPIR-V Reflect types to Vulkan types
		VkShaderStageFlagBits SpvToVkShaderStage(SpvReflectShaderStageFlagBits spvShaderStage)  const;
		VkDescriptorType      SpvToVkDescriptorType(SpvReflectDescriptorType spvDescriptorType) const;

		//Validate a new binding against the database reference binding with the same name
		bool ValidateNewBinding(const VkDescriptorSetLayoutBinding& bindingInfo, VkDescriptorType expectedDescriptorType, VkDescriptorBindingFlags expectedDescriptorFlags) const;

		//Validate a new binding against an already registered binding
		bool ValidateExistingBinding(const VkDescriptorSetLayoutBinding& newBindingInfo, const VkDescriptorSetLayoutBinding& existingBindingInfo) const;

	private:
		LoggerQueue* mLogger;

		const VkDevice        mDeviceRef;
		const SamplerManager* mSamplerManagerRef;

		//Loaded shader modules per shader path
		std::unordered_map<std::wstring, spv_reflect::ShaderModule> mLoadedShaderModules;

		//Set layout records for each shader group
		//The list of set layouts is non-owning
		std::vector<VkDescriptorSetLayout>                   mSetLayoutsFlat;
		std::vector<uint32_t>                                mSetLayoutRecordIndicesFlat;
		std::unordered_map<std::string_view, Span<uint32_t>> mSetLayoutSpansPerShaderGroup;

		//Push constants for each shader group
		//Each push constant span is lexicographically sorted by name
		std::vector<PushConstantRecord>                         mPushConstantRecords;
		std::vector<VkPushConstantRange>                        mPushConstantRanges;
		std::unordered_map<std::string_view, PushConstantSpans> mPushConstantSpansPerShaderGroup;

		//Bindings per layout
		//A lot of data is stored separately because mLayoutBindingsFlat and mLayoutBindingFlagsFlat are needed as separate arrays
		std::vector<DomainRecord>                 mDomainRecords;
		std::vector<SetLayoutRecordNode>          mSetLayoutRecordNodes;
		std::vector<VkDescriptorSetLayoutBinding> mLayoutBindingsFlat;
		std::vector<VkDescriptorBindingFlags>     mLayoutBindingFlagsFlat;
		std::vector<uint16_t>                     mLayoutBindingTypesFlat;
		
		//Domain records per pass name
		std::unordered_map<RenderPassType, BindingDomain> mPassDomainMap;

		//Binding records per binding name
		std::unordered_map<std::string_view, BindingRecord> mBindingRecordMap;
	};
}