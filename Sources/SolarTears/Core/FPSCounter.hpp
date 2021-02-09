#pragma once

#include "Timer.hpp"
#include "FrameCounter.hpp"
#include "../Logging/LoggerQueue.hpp"

class FPSCounter
{
public:
	FPSCounter();
	~FPSCounter();

	void LogFPS(const FrameCounter* frameCounter, const Timer* timer, LoggerQueue* logger);

private:
	uint64_t mLastMeasuredFrame;
	float    mLastMeasuredTime;

	float mLastMeasuredFPS;
};