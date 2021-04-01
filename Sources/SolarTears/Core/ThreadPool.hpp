#pragma once

#include <thread>
#include <vector>
#include <queue>
#include <mutex>

//Based on https://vorbrodt.blog/2019/02/26/better-code-concurrency/
//Maybe steal some ideas from https://github.com/dougbinks/enkiTS ? Hehehe
class ThreadPool
{
	using JobFunc = void(*)(void*, uint32_t);

	struct JobParameters
	{
		JobFunc   JobFunction;
		std::byte AdditionalData[56];
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