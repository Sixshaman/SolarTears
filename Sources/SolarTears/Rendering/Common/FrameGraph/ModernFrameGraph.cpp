#include "ModernFrameGraph.hpp"

ModernFrameGraph::ModernFrameGraph(FrameGraphConfig&& frameGraphConfig): mFrameGraphConfig(std::move(frameGraphConfig))
{
	mBackbufferImageSpan.Begin = 0;
	mBackbufferImageSpan.End   = 0;
}

ModernFrameGraph::~ModernFrameGraph()
{
}
