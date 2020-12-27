#pragma once

#include <unordered_set>
#include <vulkan/vulkan.h>
#include "../VulkanCUtils.hpp"
#include "../Scene/VulkanCScene.hpp"

class LoggerQueue;

namespace VulkanCBindings
{
	class FrameGraphConfig;
	class FrameGraphBuilder;

	enum class RenderPassType : uint32_t
	{
		GRAPHICS = 0, //Graphics operation on graphics queue
		COMPUTE,      //Compute  operation on graphics queue
		TRANSFER,     //Transfer operation on graphics queue
		PRESENT,      //Present  operation on present  queue

		RENDER_PASS_TYPE_COUNT
	};

	//An alternative approach would be a single class with Execute() callback and different render pass description classes.
	//This would eliminate the cost of dereferencing class pointer, then dereferencing vtable, then indexing through vtable.
	//But it would require storing all possible things a renderpass can require (pipeline, VkRenderPass object, etc.), even if pass doesn't require them.
	//To improve code cleaniness, virtual method approach was used.
	class RenderPass
	{
	public:
		RenderPass(VkDevice device, RenderPassType type = RenderPassType::GRAPHICS);
		virtual ~RenderPass();

		RenderPass(const RenderPass& right)            = delete;
		RenderPass& operator=(const RenderPass& right) = delete;

		RenderPass(RenderPass&& right)            = default;
		RenderPass& operator=(RenderPass&& right) = default;

	public:
		RenderPassType GetRenderPassType() const;

	public:
		virtual void RecordExecution(VkCommandBuffer commandBuffer, const RenderableScene* scene, const FrameGraphConfig& frameGraphConfig, uint32_t currentFrameResourceIndex) = 0;

	protected:
		const VkDevice mDeviceRef;

		RenderPassType mRenderPassType;
	};
}