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

    std::string ToString(std::span<std::string_view> strs)
    {
        if(strs.size() == 0)
        {
            return std::string();
        }

        std::string result = std::string(strs[0]);
        for(std::string_view str: strs)
        {
            result += ", " + std::string(str);
        }

        return result;
    }
}