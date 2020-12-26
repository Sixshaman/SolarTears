#include "VisualStudioDebugLogger.hpp"
#include <Windows.h>

VisualStudioDebugLogger::VisualStudioDebugLogger()
{
}

VisualStudioDebugLogger::~VisualStudioDebugLogger()
{
}

void VisualStudioDebugLogger::LogMessage(const std::string& message)
{
	OutputDebugStringA(message.c_str());
}