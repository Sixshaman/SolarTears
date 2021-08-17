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

	//Validates pass-specific bindings.
	//Returns the pass-specific set id if bindings are validated correctly and -1 if not
	//Used by passes that have pass-specific shader bindings
	using ValidatePassFunc = uint32_t(*)(std::span<VkDescriptorSetLayoutBinding> bindingSpan, std::span<std::string> nameSpan);

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
		virtual void RecordExecution(VkCommandBuffer commandBuffer, const RenderableScene* scene, const FrameGraphConfig& frameGraphConfig) const = 0;

	protected:
		const VkDevice mDeviceRef;
	};
}