#pragma once

#include <atomic>

class WaitableObject
{
public:
	WaitableObject(uint32_t jobsToWait);
	~WaitableObject();

	void NotifyJobFinished();
	void WaitForAll();

private:
	const uint32_t mJobsToWait;

	std::atomic<uint32_t> mFinishedJobsCounter;
	std::atomic_flag      mFinishedFlag;
};