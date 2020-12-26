#include "Util.hpp"
#include <cstdio>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif // _WIN32

namespace Utils
{
    //Dear Microsoft, std::format when?
    std::string StringFormat(const char* format, ...)
    {
        va_list vlist;

        va_start(vlist, format);
        int charCount = vsnprintf(nullptr, 0, format, vlist);
        va_end(vlist);

        std::string res;
        res.resize(charCount);

        va_start(vlist, format);
        vsnprintf(res.data(), res.size() + 1, format, vlist);
        va_end(vlist);

        return res;
    }

    std::wstring StringFormat(const wchar_t* format, ...)
    {
        va_list vlist;

        va_start(vlist, format);
        int charCount = _vsnwprintf_s(nullptr, 0, 0, format, vlist);
        va_end(vlist);

        std::wstring res;
        res.resize(charCount);

        va_start(vlist, format);
        _vsnwprintf_s(res.data(), res.size() + 1, res.size(), format, vlist);
        va_end(vlist);

        return res;
    }

#ifdef _WIN32
#include "..\Platform\Win32\Win32Util.inl"
#endif // _WIN32
}