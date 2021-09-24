#pragma once

#include "../ModernFrameGraphMisc.hpp"
#include <cassert>
#include <string_view>

class GBufferPassBase
{
public:
	static constexpr RenderPassClass PassClass = RenderPassClass::Graphics;
	static constexpr RenderPassType  PassType  = RenderPassType::GBufferGenerate;

	enum class PassSubresourceId: uint_fast16_t
	{
		ColorBufferImage = 0,

		Count
	};

	static inline constexpr std::string_view GetSubresourceStringId(PassSubresourceId subresourceId)
	{
		switch(subresourceId)
		{
			case PassSubresourceId::ColorBufferImage: return "GBufferColorImage";
		}

		assert(false);
		return "";
	}
};