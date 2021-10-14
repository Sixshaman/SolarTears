#pragma once

#include "../../Core/DataStructures/Span.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <span>

namespace Vulkan
{
	class DescriptorDatabase;
	class FrameGraphBuilder;

	class PassDescriptorDatabaseBuilder
	{
		struct PassSetInfo
		{
			uint32_t       PassIndex;
			uint32_t       DatabaseDescriptorIndex;
			Span<uint32_t> BindingIndexSpan;
		};

	public:
		PassDescriptorDatabaseBuilder(DescriptorDatabase* databaseToBuild);
		~PassDescriptorDatabaseBuilder();

		void AddPassSetInfo(VkDescriptorSetLayout setLayout, uint32_t passIndex, std::span<const uint16_t> setBindingTypes);

		void RecreatePassSets(const FrameGraphBuilder* frameGraphBuilder);

	private:
		void RecreatePassDescriptorPool(const FrameGraphBuilder* frameGraphBuilder);
		void AllocateDescriptorSets(std::vector<VkDescriptorSet>& outCreatedsetsPerLayout);
		void WriteDescriptorSets(const FrameGraphBuilder* frameGraphBuilder, const std::vector<VkDescriptorSet>& setsToWritePerLayout);
		void AssignDescriptorSets(const std::vector<VkDescriptorSet>& setsPerLayout);

	private:
		DescriptorDatabase* mDatabaseToBuild;

		std::vector<VkDescriptorSetLayout> mPassSetLayouts;
		std::vector<PassSetInfo>           mPassSetInfosPerLayout;
		std::vector<uint16_t>              mPassSetBindingTypes;
	};
}