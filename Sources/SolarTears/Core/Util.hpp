#pragma once

#include <string>
#include <string_view>
#include <utility>

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