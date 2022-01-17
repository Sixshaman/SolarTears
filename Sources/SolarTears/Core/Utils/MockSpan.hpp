#pragma once

#include <span>

namespace Utils
{
	//This file implements the "mock span" pattern.
	//The standard std::span is great, but sometimes I only need the (begin, end) pair, without pointing to the actual data

	//Creates a container-free span specified only by begin and end
	template<typename T>
	std::span<T> CreateMockSpan(size_t begin, size_t end)
	{
		T* mockDataStart = nullptr;
		return std::span(mockDataStart + begin, mockDataStart + end);
	}

	//Applies a mock span to the container, transforming it to the actual data range
	template<typename T, typename It>
	std::span<T> ValidateMockSpan(std::span<T> mockSpan, It rangeBegin)
	{
		const T* mockDataStart = nullptr;

		ptrdiff_t spanDataOffset = mockSpan.data() - mockDataStart;
		size_t    spanSize       = mockSpan.size();

		return std::span(rangeBegin + spanDataOffset, rangeBegin + spanDataOffset + spanSize);
	}
}
