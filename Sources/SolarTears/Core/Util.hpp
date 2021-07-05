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
	std::string  ConvertWstringToUTF8(const std::wstring_view str);
	std::wstring ConvertUTF8ToWstring(const std::string_view  str);
	
	void SystemDebugMessage(const std::string_view  str);
	void SystemDebugMessage(const std::wstring_view str);
	void SystemDebugMessage(const char*    str);
	void SystemDebugMessage(const wchar_t* str);

	std::wstring GetMainDirectory();
}