#pragma once

#include <atomic>

class WaitableObject
{
public:
	WaitableObject();
	~WaitableObject();

	void RegisterUse();
	void Notify();

	void WaitForAll();

private:
	std::atomic<uint32_t> mThreadCounter;
	uint32_t              mDesiredCounter;
};