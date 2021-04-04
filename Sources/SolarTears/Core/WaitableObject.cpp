#include "WaitableObject.hpp"

WaitableObject::WaitableObject(): mThreadCounter(0), mDesiredCounter(0)
{
}

WaitableObject::~WaitableObject()
{
}

void WaitableObject::RegisterUse()
{
	mDesiredCounter++;
}

void WaitableObject::Notify()
{
	++mThreadCounter;
	mThreadCounter.notify_one();
}

void WaitableObject::WaitForAll()
{
	mThreadCounter.wait(mDesiredCounter);
}