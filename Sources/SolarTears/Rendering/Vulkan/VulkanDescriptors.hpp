#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <span>
#include <string>
#include <numeric>
#include "VulkanDescriptorsMisc.hpp"
#include "../../Core/DataStructures/Span.hpp"

namespace Vulkan
{
	class RenderableScene;
	class SamplerManager;

	//The database for all common descriptor sets and set layouts (samplers, scene data)
	class DescriptorDatabase
	{
		friend class ShaderDatabase;

		struct SharedSetCreateMetadata
		{
			VkDescriptorSetLayout SetLayout;
			uint32_t              SetCount;
			Span<uint32_t>        BindingSpan;
		};

		struct SharedSetRecord
		{
			uint32_t FlatCreateSetIndex; //The index of the set that can be calculated from mSharedSetCreateMetadatas taking set count into account
			uint32_t SetDatabaseIndex;   //The index of the sets in mDescriptorSets
		};

	public:
		DescriptorDatabase(const VkDevice device);
		~DescriptorDatabase();

		//Creates sets from registered set layouts
		void RecreateSharedSets(const RenderableScene* sceneToCreateDescriptors, const SamplerManager* samplerManager);

	private:
		void RecreateSharedDescriptorPool(const RenderableScene* sceneToCreateDescriptors, const SamplerManager* samplerManager);
		void AllocateSharedDescriptorSets(const RenderableScene* sceneToCreateDescriptors, const SamplerManager* samplerManager, std::vector<VkDescriptorSet>& outSharedDescriptorSets);
		void UpdateSharedDescriptorSets(const RenderableScene* sceneToCreateDescriptors,   const std::vector<VkDescriptorSet>& sharedDescriptorSets);

	private:
		const VkDevice mDeviceRef;

		VkDescriptorPool mSharedDescriptorPool; //оскъ деяйпхорнпнб! оюс! оскъ деяйпхорнпнб! оскъ деяйпхорнпнб! оюс, оюс, оюс! оскъ деяйпхорнпнб!
		VkDescriptorPool mPassDescriptorPool;

		std::vector<VkDescriptorSet> mDescriptorSets;

		std::vector<SharedSetCreateMetadata>     mSharedSetCreateMetadatas; //Registered shared set create metadatas
		std::vector<SharedDescriptorBindingType> mSharedSetFormatsFlat;
		std::vector<SharedSetRecord>             mSharedSetRecords;
	};
}