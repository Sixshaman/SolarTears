#pragma once

#include "Timer.hpp"
#include "FrameCounter.hpp"

class FPSCounter
{
public:
	FPSCounter();
	~FPSCounter();

	float CalculateFPS(const FrameCounter* frameCounter, const Timer* timer);

private:
	uint64_t mLastMeasuredFrame;
	float    mLastMeasuredTime;

	float mLastMeasuredFPS;
};