#pragma once

#include "FrameGraphConfig.hpp"
#include <vector>
#include "../../../Core/DataStructures/Span.hpp"

class ModernFrameGraph
{
	friend class ModernFrameGraphBuilder;

public:
	ModernFrameGraph(const FrameGraphConfig& frameGraphConfig);
	~ModernFrameGraph();

protected:
	FrameGraphConfig mFrameGraphConfig;

	std::vector<Span<uint32_t>> mGraphicsPassSpans;

	Span<uint32_t> mBackbufferImageSpan;

	//Each i * (SwapchainFrameCount + 1) + 0 element tells the index in mRenderPasses/mImageViews that should be replaced with swapchain-related element every frame. 
	//Pass to replace is taken from mSwapchainRenderPasses[i * (SwapchainFrameCount + 1) + currentSwapchainImageIndex + 1]
	//View to replace is taken from   mSwapchainImageViews[i * (SwapchainFrameCount + 1) + currentSwapchainImageIndex + 1]
	//The reason to use it is to make mRenderPasses always relate to passes actually used
	std::vector<uint32_t> mSwapchainPassesSwapMap;
};