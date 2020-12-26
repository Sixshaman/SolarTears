#pragma once

#include <string>

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(p) (p)
#endif

#ifndef UNREFERENCED_LOCAL_VARIABLE
#define UNREFERENCED_LOCAL_VARIABLE(p) (p)
#endif

#ifndef STRING_LITERAL
#define STRING_LITERAL(x) #x
#endif

namespace Utils
{
	std::string  StringFormat(const char*    format, ...);
	std::wstring StringFormat(const wchar_t* format, ...);

	std::string ConvertWstringToUTF8(const std::wstring& str);
	
	void SystemDebugMessage(const std::string&  str);
	void SystemDebugMessage(const std::wstring& str);

	std::wstring GetMainDirectory();
}