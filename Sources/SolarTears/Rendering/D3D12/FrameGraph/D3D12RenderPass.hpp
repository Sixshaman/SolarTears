#pragma once

#include <d3d12.h>
#include "../D3D12Utils.hpp"
#include "../Scene/D3D12Scene.hpp"

class LoggerQueue;
class FrameGraphConfig;

namespace D3D12
{
	class FrameGraphBuilder;

	enum class RenderPassType : uint32_t
	{
		GRAPHICS = 0,
		COMPUTE,
		COPY,
		PRESENT,

		RENDER_PASS_TYPE_COUNT
	};

	//An alternative approach would be a single class with Execute() callback and different render pass description classes.
	//This would eliminate the cost of dereferencing class pointer, then dereferencing vtable, then indexing through vtable.
	//But it would require storing all possible things a renderpass can require (pipeline, VkRenderPass object, etc.), even if pass doesn't require them.
	//To improve code cleaniness, virtual method approach was used.
	class RenderPass
	{
	public:
		RenderPass();
		virtual ~RenderPass();

		RenderPass(const RenderPass& right)            = delete;
		RenderPass& operator=(const RenderPass& right) = delete;

		RenderPass(RenderPass&& right)            = default;
		RenderPass& operator=(RenderPass&& right) = default;

	public:
		virtual ID3D12PipelineState* FirstPipeline() const = 0;

		virtual void RecordExecution(ID3D12GraphicsCommandList6* commandList, const RenderableScene* scene, const FrameGraphConfig& frameGraphConfig) const = 0;
	};
}