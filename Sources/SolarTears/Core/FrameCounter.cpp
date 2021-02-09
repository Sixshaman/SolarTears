#include "FrameCounter.hpp"

FrameCounter::FrameCounter()
{
	mFrameCount = 0;
}

FrameCounter::~FrameCounter()
{
}

void FrameCounter::IncrementFrame()
{
	mFrameCount++;
}

uint64_t FrameCounter::GetFrameCount() const
{
	return mFrameCount;
}
