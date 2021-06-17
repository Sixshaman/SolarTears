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

	struct BarrierSpan
	{
		uint32_t BeforePassBegin;
		uint32_t BeforePassEnd;
		uint32_t AfterPassBegin;
		uint32_t AfterPassEnd;
	};

	struct MultiframeBarrierInfo
	{
		uint32_t BarrierIndex;
		uint32_t BaseTexIndex;
		uint8_t  TexturePeriod;
	};

public:
	ModernFrameGraph(const FrameGraphConfig& frameGraphConfig);
	~ModernFrameGraph();

protected:
	FrameGraphConfig mFrameGraphConfig;

	std::vector<Span<uint32_t>> mGraphicsPassSpans;

	std::vector<RenderPassSpanInfo> mPassFrameSpans;

	std::vector<BarrierSpan>           mRenderPassBarriers; //Required barriers before ith pass are mResourceBarriers[Span.Begin...Span.End], where Span is mRenderPassBarriers[i]
	std::vector<MultiframeBarrierInfo> mMultiframeBarrierInfos;

	Span<uint32_t> mBackbufferImageSpan;
};