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
		friend class SharedDescriptorDatabaseBuilder;
		friend class PassDescriptorDatabaseBuilder;

		struct SharedSetCreateMetadata
		{
			VkDescriptorSetLayout SetLayout;
			Span<uint32_t>        BindingSpan;
		};

		struct SharedSetRecord
		{
			uint32_t SetCreateMetadataIndex; //The index of the set in mSharedSetCreateMetadatas
			uint32_t SetDatabaseIndex;       //The index of the sets in mDescriptorSets
		};

	public:
		DescriptorDatabase(const VkDevice device);
		~DescriptorDatabase();

		void ClearDatabase();

		std::span<VkDescriptorSet> ValidateSetSpan(std::span<VkDescriptorSet> setToValidate, const VkDescriptorSet* originalSpanStartPoint);

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