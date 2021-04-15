#include "FrameGraphConfig.hpp"

FrameGraphConfig::FrameGraphConfig()
{
	mViewportWidth  = 256;
	mViewportHeight = 256;
}

FrameGraphConfig::~FrameGraphConfig()
{
}

void FrameGraphConfig::SetScreenSize(uint16_t width, uint16_t height)
{
	mViewportWidth  = width;
	mViewportHeight = height;
}

uint16_t FrameGraphConfig::GetViewportWidth() const
{
	return mViewportWidth;
}

uint16_t FrameGraphConfig::GetViewportHeight() const
{
	return mViewportHeight;
}
