#include "VulkanCFrameGraphConfig.hpp"

VulkanCBindings::FrameGraphConfig::FrameGraphConfig()
{
	mViewportWidth  = 256;
	mViewportHeight = 256;
}

VulkanCBindings::FrameGraphConfig::~FrameGraphConfig()
{
}

void VulkanCBindings::FrameGraphConfig::SetScreenSize(uint16_t width, uint16_t height)
{
	mViewportWidth  = width;
	mViewportHeight = height;
}

uint16_t VulkanCBindings::FrameGraphConfig::GetViewportWidth() const
{
	return mViewportWidth;
}

uint16_t VulkanCBindings::FrameGraphConfig::GetViewportHeight() const
{
	return mViewportHeight;
}
