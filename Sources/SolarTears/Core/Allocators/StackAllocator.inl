template<typename T, typename ...ConstructorArgs>
inline T* StackAllocator::allocate(ConstructorArgs... args)
{
	void* placementPointer = allocInternal(sizeof(T), alignof(T));
	return new(placementPointer) T(std::forward<ConstructorArgs>(args)...);
}

template<typename T, typename ...ConstructorArgs>
inline T* StackAllocator::allocateAligned(size_t alignment, ConstructorArgs... args)
{
	void* placementPointer = allocInternal(sizeof(T), alignment);
	return new(placementPointer) T(std::forward<ConstructorArgs>(args)...);
}