#include "ModernFrameGraph.hpp"

ModernFrameGraph::ModernFrameGraph(FrameGraphConfig&& frameGraphConfig): mFrameGraphConfig(std::move(frameGraphConfig))
{
}

ModernFrameGraph::~ModernFrameGraph()
{
}


uint32_t ModernFrameGraph::CalcPassIndex(const PassFrameSpan& passFrameSpan, uint32_t swapchainImageIndex, uint32_t frameIndex) const
{
	switch(passFrameSpan.SwapType)
	{
	case RenderPassFrameSwapType::PerLinearFrame:
		return passFrameSpan.Begin + frameIndex % (passFrameSpan.End - passFrameSpan.Begin);

	case RenderPassFrameSwapType::PerBackbufferImage:
		return passFrameSpan.Begin + swapchainImageIndex;

	default:
		break;
	}

	return passFrameSpan.Begin;
}