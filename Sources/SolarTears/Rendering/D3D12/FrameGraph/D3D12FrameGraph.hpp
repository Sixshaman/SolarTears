#pragma once

#include <d3d12.h>
#include "../../FrameGraphConfig.hpp"

namespace D3D12
{
	class FrameGraph
	{
		friend class FrameGraphDescriptorCreator;

	public:
		FrameGraph(const FrameGraphConfig& frameGraphConfig);
		~FrameGraph();

	private:
		FrameGraphConfig mFrameGraphConfig;
	};
}