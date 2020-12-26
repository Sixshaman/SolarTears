#pragma once

#include <cstdint>

namespace VulkanCBindings
{
	class FrameGraphConfig
	{
	public:
		FrameGraphConfig();
		~FrameGraphConfig();

	public:
		void SetScreenSize(uint16_t width, uint16_t height);

	public:
		uint16_t GetViewportWidth() const;
		uint16_t GetViewportHeight() const;

	private:
		uint16_t mViewportWidth;
		uint16_t mViewportHeight;
	};
}
