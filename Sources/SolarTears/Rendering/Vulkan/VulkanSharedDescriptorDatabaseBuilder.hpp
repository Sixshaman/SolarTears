#pragma once

#include "../../Core/DataStructures/Span.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <span>

namespace Vulkan
{
	class DescriptorDatabase;
	class RenderableScene;
	class SamplerManager;

	class SharedDescriptorDatabaseBuilder
	{
	public:
		SharedDescriptorDatabaseBuilder(DescriptorDatabase* databaseToBuild);
		~SharedDescriptorDatabaseBuilder();

		void     ClearRegisteredSharedSetInfos();
		uint32_t RegisterSharedSetInfo(VkDescriptorSetLayout setLayout, std::span<const uint16_t> setBindings);
		void     AddSharedSetInfo(uint32_t setMetadataId);

		//Creates sets from registered set layouts
		void RecreateSharedSets(const RenderableScene* scene, const SamplerManager* samplerManager);

	private:
		void RecreateSharedDescriptorPool(const RenderableScene* scene, const SamplerManager* samplerManager);
		void AllocateSharedDescriptorSets(const RenderableScene* scene, const SamplerManager* samplerManager, std::vector<VkDescriptorSet>& outAllocatedSets);
		void UpdateSharedDescriptorSets(const RenderableScene* scene, const std::vector<VkDescriptorSet>& setsToWritePerCreateMetadata);
		void AssignSharedDescriptorSets(const std::vector<VkDescriptorSet>& setsPerCreateMetadata);

	private:
		DescriptorDatabase* mDatabaseToBuild;
	};
}