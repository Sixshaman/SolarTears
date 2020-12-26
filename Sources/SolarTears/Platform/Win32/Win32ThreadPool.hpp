#pragma once

#include <Windows.h>
#include <wil/resource.h>
#include <cstdint>

class ThreadPool
{
	using ThreadFunc = void(*)(void*);

	ThreadPool(uint32_t numOfThreads);
	~ThreadPool();

	void DoWork(ThreadFunc func, void* userData);

};