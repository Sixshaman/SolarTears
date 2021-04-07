#include "WaitableObject.hpp"

WaitableObject::WaitableObject(uint32_t jobsToWait): mFinishedJobsCounter(0), mJobsToWait(jobsToWait)
{
}

WaitableObject::~WaitableObject()
{
}

void WaitableObject::NotifyJobFinished()
{
	if(++mFinishedJobsCounter >= mJobsToWait)
	{
		mFinishedFlag.test_and_set();
		mFinishedFlag.notify_all();
	}
}

void WaitableObject::WaitForAll()
{
	//If you pass mFinishedJobsCounter >= mJobsToWait instead of just false, a deadlock can occur
	//Consider a case: other thread has already set the flag. This way mFinishedFlag is already true.
	//But mFinishedJobsCounter >= mJobsToWait is also true. This means the flag waits for the opposite thing
	mFinishedFlag.wait(false);
}