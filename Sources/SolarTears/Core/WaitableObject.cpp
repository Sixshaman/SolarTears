#include "WaitableObject.hpp"

WaitableObject::WaitableObject(): mThreadCounter(0), mWaitCounter(0)
{
}

WaitableObject::~WaitableObject()
{
}

void WaitableObject::RegisterUse()
{
	mWaitCounter++;
}

void WaitableObject::Notify()
{
	if(++mThreadCounter >= mWaitCounter)
	{
		mFinishedFlag.test_and_set();
		mFinishedFlag.notify_all();
	}
}

void WaitableObject::WaitForAll()
{
	//If you pass mThreadCounter >= mWaitCounter instead of just false, a deadlock can occur
	//Consider a case: other thread has already set the flag. This way mFinishedFlag is already true.
	//But mThreadCounter >= mWaitCounter is also true. This means the flag waits for the opposite thing
	mFinishedFlag.wait(false);
}