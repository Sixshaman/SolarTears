#pragma once

#include "ModernFrameGraphMisc.hpp"
#include <cassert>

template<typename Pass>
uint32_t GetPassResourceCount()
{
	return Pass::GetPassResourceCount();
}

uint32_t GetPassResourceCount(RenderPassType passType)
{
	switch(passType)
	{
	case RenderPassType::GBufferGenerate:
		return GetPassResourceCount<RenderPassType::GBufferGenerate>();

	case RenderPassType::GBufferDraw:
		return GetPassResourceCount<RenderPassType::GBufferDraw>();

	case RenderPassType::CopyImage:
		return GetPassResourceCount<RenderPassType::CopyImage>();

	default:
		assert(false);
		break;
	}
}