#include "FPSCounter.hpp"

FPSCounter::FPSCounter()
{
	mLastMeasuredTime  = 0.0f;
	mLastMeasuredFrame = 0;

	mLastMeasuredFPS = 60.0f;
}

FPSCounter::~FPSCounter()
{
}

float FPSCounter::CalculateFPS(const FrameCounter* frameCounter, const Timer* timer)
{
	float    currMeasurementTime = timer->GetCurrTime();
	uint64_t frameIndex          = frameCounter->GetFrameCount();

	if(currMeasurementTime - mLastMeasuredTime >= 1.0f)
	{
		mLastMeasuredFPS   = (frameIndex - mLastMeasuredFrame) / (currMeasurementTime - mLastMeasuredTime);
		mLastMeasuredTime  = currMeasurementTime;
		mLastMeasuredFrame = frameIndex;	 
	}

	return mLastMeasuredFPS;
}
