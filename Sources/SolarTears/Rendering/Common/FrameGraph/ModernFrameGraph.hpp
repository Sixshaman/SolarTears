#pragma once

#include "FrameGraphConfig.hpp"
#include <vector>

class ModernFrameGraph
{
	friend class ModernFrameGraphBuilder;

	struct DependencyLevelSpan
	{
		uint32_t DependencyLevelBegin;
		uint32_t DependencyLevelEnd;
	};

public:
	ModernFrameGraph();
	~ModernFrameGraph();

protected:
	FrameGraphConfig mFrameGraphConfig;

	std::vector<DependencyLevelSpan> mGraphicsPassSpans;
};