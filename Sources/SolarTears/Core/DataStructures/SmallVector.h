#pragma once

#include <iterator>
#include <cassert>

template<typename T, size_t MaxSize>
class SmallVector
{
public:
	class iterator : public std::iterator<std::input_iterator_tag, T>
	{
	public:
		explicit iterator(T* ref): mIterRef(ref)
		{
		}

		iterator& operator++()
		{
			mIterRef = mIterRef + 1;
			return *this;
		}

		iterator operator++(int)
		{
			iterator temp = *this;
			++(*this);
			return temp;
		}

		bool operator==(const iterator other) const
		{
			return mIterRef == other.mIterRef;
		}

		bool operator!=(const iterator other) const
		{
			return mIterRef != other.mIterRef;
		}

		T& operator*() const
		{
			return *mIterRef;
		}

	private:
		T* mIterRef = mFirst;
	};

public:
	SmallVector();
	~SmallVector();

	void push_back(const T& item);

	T& back()                         const;
	T& operator[](const size_t index) const;

	iterator begin();
	iterator end();

	std::reverse_iterator<iterator> rbegin();
	std::reverse_iterator<iterator> rend();

private:
	T mItems[MaxSize];

	T* mFirst;
	T* mLast;
};

template<typename T, size_t MaxSize>
inline SmallVector<T, MaxSize>::SmallVector(): mFirst(&mItems[0]), mLast(&mItems[0])
{
}

template<typename T, size_t MaxSize>
inline SmallVector<T, MaxSize>::~SmallVector()
{
}

template<typename T, size_t MaxSize>
inline void SmallVector<T, MaxSize>::push_back(const T& item)
{
	assert(std::distance(mFirst, mLast) < MaxSize);

	*mLast = item;
	mLast++;
}

template<typename T, size_t MaxSize>
inline T& SmallVector<T, MaxSize>::back() const
{
	assert(mFirst != mLast);
	return *(mLast - 1);
}

template<typename T, size_t MaxSize>
inline T& SmallVector<T, MaxSize>::operator[](const size_t index) const
{
	return *(mFirst + index);
}

template<typename T, size_t MaxSize>
inline SmallVector<T, MaxSize>::iterator SmallVector<T, MaxSize>::begin()
{
	return iterator(mFirst);
}

template<typename T, size_t MaxSize>
inline SmallVector<T, MaxSize>::iterator SmallVector<T, MaxSize>::end()
{
	return iterator(mLast);
}

template<typename T, size_t MaxSize>
inline std::reverse_iterator<SmallVector<T, MaxSize>::iterator> SmallVector<T, MaxSize>::rbegin()
{
	return std::reverse_iterator<SmallVector<T, MaxSize>::iterator>(begin());
}

template<typename T, size_t MaxSize>
inline std::reverse_iterator<SmallVector<T, MaxSize>::iterator> SmallVector<T, MaxSize>::rend()
{
	return std::reverse_iterator<SmallVector<T, MaxSize>::iterator>(end());
}
                 