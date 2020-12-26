#pragma once

#include <new>
#include "..\..\Logging\LoggerQueue.hpp"

class StackAllocator //Allocates static objects that live the whole time of the application
{
	static const std::align_val_t MainStackAllocatorAlignment = std::align_val_t(1048576); //1MB is the default alignment for the whole allocator

public:
	StackAllocator(size_t maxBufferSize);
	~StackAllocator();

	template <typename T, typename ...ConstructorArgs>
	T* allocate(ConstructorArgs... args);

	template <typename T, typename ...ConstructorArgs>
	T* allocateAligned(size_t alignment, ConstructorArgs... args);

private:
	void* allocInternal(size_t size, size_t alignment);

	StackAllocator(const StackAllocator& al)			= delete;
	StackAllocator& operator=(const StackAllocator& al) = delete;

private:
	std::byte* mMemoryPointer;

	size_t mMemorySize;
	size_t mMaxSize;

	LoggerQueue* mLoggingBoard;
};

#include "StackAllocator.inl"