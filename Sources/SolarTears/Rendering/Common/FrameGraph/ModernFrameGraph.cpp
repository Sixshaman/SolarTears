#include "ModernFrameGraph.hpp"

ModernFrameGraph::ModernFrameGraph(FrameGraphConfig&& frameGraphConfig): mFrameGraphConfig(std::move(frameGraphConfig))
{
}

ModernFrameGraph::~ModernFrameGraph()
{
}
