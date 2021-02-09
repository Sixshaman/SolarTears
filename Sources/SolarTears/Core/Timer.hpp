#pragma once

#include <chrono>

class Timer
{
public:
	Timer();
	~Timer();

	void Tick();

	float GetCurrTime()  const;
	float GetDeltaTime() const;

private:
	std::chrono::high_resolution_clock mClock;

	std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::nanoseconds> mStartTime;
	std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::nanoseconds> mPrevTime;
	std::chrono::time_point<std::chrono::high_resolution_clock, std::chrono::nanoseconds> mCurrTime;
};