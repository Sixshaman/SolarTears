std::string ConvertWstringToUTF8(const std::wstring& str)
{
	std::string result;
	int charCount = WideCharToMultiByte(CP_UTF8, WC_COMPOSITECHECK, str.c_str(), -1, nullptr, 0, nullptr, nullptr);

	result.resize(charCount - 1);
	WideCharToMultiByte(CP_UTF8, WC_COMPOSITECHECK, str.c_str(), -1, result.data(), (int)result.size(), nullptr, nullptr);

	return result;
}

std::wstring ConvertUTF8ToWstring(const std::string& str)
{
	std::wstring result;
	int charCount = MultiByteToWideChar(CP_UTF8, MB_COMPOSITE, str.c_str(), -1, nullptr, 0);

	result.resize(charCount - 1);
	MultiByteToWideChar(CP_UTF8, WC_COMPOSITECHECK, str.c_str(), -1, result.data(), (int)result.size());

	return result;
}

void SystemDebugMessage(const std::string& str)
{
#if defined(DEBUG) || defined(_DEBUG)
	OutputDebugStringA(str.c_str());
	OutputDebugStringA("\n");
#else
	UNREFERENCED_PARAMETER(str);
#endif
}

void SystemDebugMessage(const std::wstring& str)
{
#if defined(DEBUG) || defined(_DEBUG)
	OutputDebugStringW(str.c_str());
	OutputDebugStringW(L"\n");
#else
	UNREFERENCED_PARAMETER(str);
#endif
}

std::wstring GetMainDirectory()
{
	//TODO: only call it once
	std::vector<wchar_t> mainDir;
	mainDir.resize(MAX_PATH);

	while(GetModuleFileName(nullptr, mainDir.data(), (DWORD)mainDir.size()) == mainDir.size())
	{
		mainDir.resize((size_t)(mainDir.size() * 1.5f));
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