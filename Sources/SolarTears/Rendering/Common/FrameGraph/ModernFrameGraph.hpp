#pragma once

#include "FrameGraphConfig.hpp"
#include <vector>
#include "../../../Core/DataStructures/Span.hpp"

class ModernFrameGraph
{
	friend class ModernFrameGraphBuilder;

protected:
	struct BarrierSpan
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

	std::vector<Span<uint32_t>> mGraphicsPassSpans;

	std::vector<BarrierSpan> mRenderPassBarriers; //The ith element refers to barrier spans for ith render pass. The last element refers to barrier spans for swapchain acquire-present "pass"

	Span<uint32_t> mBackbufferImageSpan;
};