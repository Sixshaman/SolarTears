#pragma once

#include "FrameGraphConfig.hpp"
#include "ModernFrameGraphMisc.hpp"
#include <vector>
#include "../../../Core/DataStructures/Span.hpp"

class ModernFrameGraph
{
	friend class ModernFrameGraphBuilder;

protected:
	struct PassFrameSpan //Describes a set of passes substituting for different frames
	{
		uint32_t                Begin;
		uint32_t                End;
		RenderPassFrameSwapType SwapType;
	};

	struct BarrierPassSpan
	{
		uint32_t BeforePassBegin;
		uint32_t BeforePassEnd;
		uint32_t AfterPassBegin;
		uint32_t AfterPassEnd;
	};

public:
	ModernFrameGraph(FrameGraphConfig&& frameGraphConfig);
	~ModernFrameGraph();

protected:
	FrameGraphConfig mFrameGraphConfig;

	std::vector<BarrierPassSpan> mRenderPassBarriers; //The ith element refers to barrier spans for ith render pass. The last element refers to barrier spans for swapchain acquire-present "pass"

	std::vector<PassFrameSpan>  mFrameSpansPerRenderPass;
	std::vector<Span<uint32_t>> mGraphicsPassSpansPerDependencyLevel;
};