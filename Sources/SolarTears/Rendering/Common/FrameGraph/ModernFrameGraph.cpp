#include "ModernFrameGraph.hpp"

ModernFrameGraph::ModernFrameGraph(const FrameGraphConfig& frameGraphConfig): mFrameGraphConfig(frameGraphConfig)
{
	mBackbufferImageSpanBegin = 0;
	mBackbufferImageSpanEnd   = 0;
}

ModernFrameGraph::~ModernFrameGraph()
{
}
