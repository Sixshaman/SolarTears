#include "Util.hpp"
#include <cstdio>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif // _WIN32

namespace Utils
{
#ifdef _WIN32
#include "..\Platform\Win32\Win32Util.inl"
#endif // _WIN32
}