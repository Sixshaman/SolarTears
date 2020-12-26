#pragma once

#include <string>

class Logger
{
public:
	Logger();
	virtual ~Logger();

	virtual void LogMessage(const std::string& message);
};