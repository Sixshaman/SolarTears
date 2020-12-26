#include "Logger.hpp"
#include "..\Core\Util.hpp"

Logger::Logger()
{
}

Logger::~Logger()
{
}

void Logger::LogMessage(const std::string& message)
{
	//Do nothing, default logger is a dummy logger
	UNREFERENCED_PARAMETER(message);
}
