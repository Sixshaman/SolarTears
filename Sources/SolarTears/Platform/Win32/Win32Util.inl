std::string ConvertWstringToUTF8(const std::wstring_view str)
{
	std::string result;
	int charCount = WideCharToMultiByte(CP_UTF8, WC_COMPOSITECHECK, str.data(), (int)str.size(), nullptr, 0, nullptr, nullptr);

	result.resize(charCount);
	WideCharToMultiByte(CP_UTF8, WC_COMPOSITECHECK, str.data(), (int)str.size(), result.data(), (int)result.size(), nullptr, nullptr);

	return result;
}

std::wstring ConvertUTF8ToWstring(const std::string_view str)
{
	std::wstring result;
	int charCount = MultiByteToWideChar(CP_UTF8, MB_COMPOSITE, str.data(), (int)str.size(), nullptr, 0);

	result.resize(charCount);
	MultiByteToWideChar(CP_UTF8, MB_COMPOSITE, str.data(), (int)str.size(), result.data(), (int)result.size());

	return result;
}

void SystemDebugMessage(const std::string_view str)
{
	SystemDebugMessage(str.data());
}

void SystemDebugMessage(const std::wstring_view str)
{
	SystemDebugMessage(str.data());
}

void SystemDebugMessage(const char* str)
{
	OutputDebugStringA(str);
}

void SystemDebugMessage(const wchar_t* str)
{
	OutputDebugStringW(str);
}

std::wstring GetMainDirectory()
{
	//TODO: only call it once
	std::vector<wchar_t> mainDir;
	mainDir.resize(MAX_PATH);

	while(GetModuleFileName(nullptr, mainDir.data(), (DWORD)mainDir.size()) == mainDir.size())
	{
		mainDir.resize((size_t)(mainDir.size() * 1.5));
	}

	std::vector<wchar_t> drive;
	drive.resize(10);

	std::vector<wchar_t> dir;
	dir.resize(mainDir.size());

	_wsplitpath_s(mainDir.data(), drive.data(), drive.size(), dir.data(), dir.size(), nullptr, 0, nullptr, 0);

	//Otherwise doesn't work
	std::wstring res;
	res.append(drive.data());
	res.append(dir.data());

	return res;
}