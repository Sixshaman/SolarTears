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

void FPSCounter::LogFPS(const FrameCounter* frameCounter, const Timer* timer, LoggerQueue* logger)
{
	float    currMeasurementTime = timer->GetCurrTime();
	uint64_t frameIndex          = frameCounter->GetFrameCount();

	if(currMeasurementTime - mLastMeasuredTime >= 1.0f)
	{
		mLastMeasuredFPS   = (frameIndex - mLastMeasuredFrame) / (currMeasurementTime - mLastMeasuredTime);
		mLastMeasuredTime  = currMeasurementTime;
		mLastMeasuredFrame = frameIndex;	 

		logger->PostLogMessage("FPS: " + std::to_string(mLastMeasuredFPS) + ", mspf: " + std::to_string(1000.0f / mLastMeasuredFPS));
	}
}
