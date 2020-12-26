#include "StackAllocator.hpp"
#include <new>
#include <cstdint>

StackAllocator::StackAllocator(size_t maxBufferSize): mMemoryPointer(nullptr), mMemorySize(0), mMaxSize(maxBufferSize)
{
	mMemoryPointer = (std::byte*)(::operator new(mMaxSize, MainStackAllocatorAlignment));
}

StackAllocator::~StackAllocator()
{
	::operator delete(mMemoryPointer);
}

void* StackAllocator::allocInternal(size_t size, size_t alignment)
{
	if(alignment > (size_t)MainStackAllocatorAlignment) //Cannot allocate larger aligned memory
	{
		throw std::bad_alloc();
	}

	size_t pad = (alignment - mMemorySize % alignment) % alignment;
	if(mMemorySize + size + pad > mMaxSize)
	{
		throw std::bad_alloc();
	}

	mMemorySize += pad;

	void* allocPointer = mMemoryPointer + mMemorySize;

	mMemorySize += size;
	return allocPointer;
}
