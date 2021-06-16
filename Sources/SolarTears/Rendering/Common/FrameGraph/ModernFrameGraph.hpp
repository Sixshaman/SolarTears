#pragma once

#include "FrameGraphConfig.hpp"
#include <vector>
#include "../../../Core/DataStructures/Span.hpp"

class ModernFrameGraph
{
	friend class ModernFrameGraphBuilder;

protected:
	struct RenderPassSpanInfo
	{
		uint32_t PassSpanBegin; //The start of per-frame passes in the frame graph pass list. The span describes the range of pass objects using the same pass but different per-frame resources
		uint32_t OwnFrames;     //The number of per-pass own frames (excluding swapchain-related ones)
	};

public:
	ModernFrameGraph(const FrameGraphConfig& frameGraphConfig);
	~ModernFrameGraph();

protected:
	FrameGraphConfig mFrameGraphConfig;

	std::vector<Span<uint32_t>> mGraphicsPassSpans;

	std::vector<RenderPassSpanInfo> mPassFrameSpans;

	Span<uint32_t> mBackbufferImageSpan;
};