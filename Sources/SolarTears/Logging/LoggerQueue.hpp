#pragma once

#include <deque>
#include <string>

class Logger;

class LoggerQueue
{
public:
	LoggerQueue();
	~LoggerQueue();

	void PostLogMessage(const std::string&  message);
	void PostLogMessage(const std::wstring& message);

	void FeedMessages(Logger* logger, uint32_t maxCount);

private:
	std::deque<std::string> mMessageQueue;
};