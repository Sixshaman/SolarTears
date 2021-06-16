#include "ModernFrameGraph.hpp"

ModernFrameGraph::ModernFrameGraph(const FrameGraphConfig& frameGraphConfig): mFrameGraphConfig(frameGraphConfig)
{
	mBackbufferImageSpan.Begin = 0;
	mBackbufferImageSpan.End   = 0;
}

ModernFrameGraph::~ModernFrameGraph()
{
}
