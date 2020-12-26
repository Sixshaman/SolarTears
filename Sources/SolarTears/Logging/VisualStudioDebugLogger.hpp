#pragma once

#include "Logger.hpp"

class VisualStudioDebugLogger: public Logger
{
public:
	VisualStudioDebugLogger();
	~VisualStudioDebugLogger();

	virtual void LogMessage(const std::string& message);
};