#include "Timer.hpp"

Timer::Timer()
{
	mStartTime = std::chrono::time_point_cast<std::chrono::seconds>(mClock.now());
	mPrevTime  = std::chrono::time_point_cast<std::chrono::seconds>(mClock.now());
	mCurrTime  = std::chrono::time_point_cast<std::chrono::seconds>(mClock.now());
}

Timer::~Timer()
{
}

void Timer::Tick()
{
	mPrevTime = mCurrTime;
	mCurrTime = std::chrono::time_point_cast<std::chrono::seconds>(mClock.now());
}

float Timer::GetCurrTime()
{
	return static_cast<float>((mCurrTime - mStartTime).count() / (1000000000.0));
}

float Timer::GetDeltaTime()
{
	return static_cast<float>((mCurrTime - mPrevTime).count() / (1000000000.0));
}
