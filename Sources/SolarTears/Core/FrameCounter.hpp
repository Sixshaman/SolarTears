#pragma once

#include <chrono>

class FrameCounter
{
public:
	FrameCounter();
	~FrameCounter();

	void IncrementFrame();

	uint64_t GetFrameCount() const;

private:
	uint64_t mFrameCount;
};