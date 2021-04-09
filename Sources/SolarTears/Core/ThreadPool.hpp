#pragma once

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "WaitableObject.hpp"

//Based on https://vorbrodt.blog/2019/02/26/better-code-concurrency/
class ThreadPool
{
	using JobFunc = void(*)(void*, uint32_t);

	struct JobParameters
	{
		JobFunc   JobFunction;
		uint32_t  AdditionalDataSize;
		std::byte AdditionalData[52];
	};

public:
	ThreadPool(uint_fast16_t numOfThreads = (GetHardwareThreads() - 1));
	~ThreadPool();

	static uint32_t GetHardwareThreads();

	uint32_t GetWorkerThreadCount() const;

	void EnqueueWork(JobFunc func, void* userData, size_t userDataSize);

private:
	std::vector<std::thread>               mThreads;
	std::vector<std::queue<JobParameters>> mThreadQueues;
	std::vector<std::mutex>                mQueueMutexes;

	size_t                        mLastTaskedThread;
	std::vector<std::atomic_bool> mThreadFinishFlags;
};