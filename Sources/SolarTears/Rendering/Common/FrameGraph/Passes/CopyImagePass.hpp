#pragma once

#include "../ModernFrameGraphMisc.hpp"
#include <cassert>
#include <string_view>

class CopyImagePassBase
{
public:
	static constexpr RenderPassClass PassClass = RenderPassClass::Transfer;
	static constexpr RenderPassType  PassType  = RenderPassType::CopyImage;

	enum class PassSubresourceId: uint_fast16_t
	{
		SrcImage = 0,
		DstImage,

		Count
	};

	static inline constexpr std::string_view GetSubresourceStringId(PassSubresourceId subresourceId)
	{
		switch(subresourceId)
		{
			case PassSubresourceId::SrcImage: return "CopySrcImage";
			case PassSubresourceId::DstImage: return "CopyDstImage";
		}

		assert(false);
		return "";
	}
};