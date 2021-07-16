#include "Logger.hpp"
#include "..\Core\Util.hpp"

Logger::Logger()
{
}

Logger::~Logger()
{
}

void Logger::LogMessage([[maybe_unused]] const std::string& message)
{
	//Do nothing, default logger is a dummy logger
}
