#pragma once

#include <unordered_set>
#include <vulkan/vulkan.h>
#include "../VulkanUtils.hpp"

class LoggerQueue;
class FrameGraphConfig;

namespace Vulkan
{
	class FrameGraphBuilder;
	class RenderableScene;
	class ShaderDatabase;
	class SharedDescriptorDatabaseBuilder;
	class PassDescriptorDatabaseBuilder;

	//An alternative approach would be a single class with Execute() callback and different render pass description classes.
	//This would eliminate the cost of dereferencing class pointer, then dereferencing vtable, then indexing through vtable.
	//But it would require storing all possible things a renderpass can require (pipeline, VkRenderPass object, etc.), even if pass doesn't require them.
	//To improve code cleaniness, virtual method approach was used.
	class RenderPass
	{
	public:
		RenderPass(VkDevice device);
		virtual ~RenderPass();

		RenderPass(const RenderPass& right)            = delete;
		RenderPass& operator=(const RenderPass& right) = delete;

		RenderPass(RenderPass&& right)            = default;
		RenderPass& operator=(RenderPass&& right) = default;

	public:
		virtual void RecordExecution(VkCommandBuffer commandBuffer, const RenderableScene* scene, const FrameGraphConfig& frameGraphConfig, uint32_t frameResourceIndex) const = 0;

		virtual void ValidateDescriptorSets(const ShaderDatabase* shaderDatabase, SharedDescriptorDatabaseBuilder* sharedDatabaseBuilder, PassDescriptorDatabaseBuilder* passDatabaseBuilder) = 0;

	protected:
		const VkDevice mDeviceRef;
	};
}