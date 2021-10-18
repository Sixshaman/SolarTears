#pragma once

#include "../ModernFrameGraphMisc.hpp"
#include <cassert>
#include <string_view>
#include <array>

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

	constexpr static std::array WriteSubresourceIds =
	{
		PassSubresourceId::ColorBufferImage
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