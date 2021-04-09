#include "ThreadPool.hpp"

#include <Windows.h>
#include <string>
#include <cassert>

ThreadPool::ThreadPool(uint_fast16_t numOfThreads): mQueueMutexes(numOfThreads), mThreadQueues(numOfThreads), mThreadFinishFlags(numOfThreads), mLastTaskedThread(0)
{
	static_assert(sizeof(JobParameters) == 64);

	assert(numOfThreads <= 65535); //Support only 2^16 threads

	for(uint32_t threadIndex = 0; threadIndex < numOfThreads; threadIndex++)
	{
		mThreadFinishFlags[threadIndex] = false;

		auto workerFunc = [threadIndex, numOfThreads, this]()
		{
			while(true)
			{
				if(mThreadFinishFlags[threadIndex])
				{
					break;
				}

				//Steal the task
				size_t currentQueue = threadIndex;
				while(!mQueueMutexes[currentQueue].try_lock())
				{
					currentQueue = (currentQueue + 1) % numOfThreads;
				}

				std::queue<JobParameters>& threadQueue = mThreadQueues[currentQueue];
				if(!threadQueue.empty())
				{
					//Take the task and dispatch it
					JobParameters jobParams = threadQueue.front();
					threadQueue.pop();

					mQueueMutexes[currentQueue].unlock();	
					jobParams.JobFunction(jobParams.AdditionalData, jobParams.AdditionalDataSize);
				}
				else
				{
					//Let other system threads use this core
					mQueueMutexes[currentQueue].unlock();
					std::this_thread::yield();
				}
			}
		};
		
		mThreads.emplace_back(workerFunc);
	}
}

ThreadPool::~ThreadPool()
{
	for(size_t q = 0; q < mThreadFinishFlags.size(); q++)
	{
		mThreadFinishFlags[q] = true;
	}

	for(size_t i = 0; i < mThreads.size(); i++)
	{
		mThreads[i].join();
	}
}

uint32_t ThreadPool::GetHardwareThreads()
{
	return std::thread::hardware_concurrency();
}

uint32_t ThreadPool::GetWorkerThreadCount() const
{
	return uint32_t(mThreads.size());
}

void ThreadPool::EnqueueWork(JobFunc func, void* userData, size_t userDataSize)
{
	assert(userDataSize < sizeof(JobParameters::AdditionalData));

	size_t currentQueue = (mLastTaskedThread + 1) % mQueueMutexes.size();
	while(!mQueueMutexes[currentQueue].try_lock())
	{
		currentQueue = (currentQueue + 1) % mQueueMutexes.size();
	}

	mThreadQueues[currentQueue].push(JobParameters{.JobFunction = func, .AdditionalDataSize = (uint32_t)userDataSize});
	memcpy(mThreadQueues[currentQueue].back().AdditionalData, userData, userDataSize);

	mQueueMutexes[currentQueue].unlock();
}