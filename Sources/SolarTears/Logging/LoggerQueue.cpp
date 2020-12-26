#include "LoggerQueue.hpp"
#include "..\Core\Util.hpp"
#include "Logger.hpp"

LoggerQueue::LoggerQueue()
{
}

LoggerQueue::~LoggerQueue()
{
}

void LoggerQueue::PostLogMessage(const std::string& message)
{
	mMessageQueue.push_front(message + "\n");
}

void LoggerQueue::PostLogMessage(const std::wstring& message)
{
	PostLogMessage(Utils::ConvertWstringToUTF8(message));
}

void LoggerQueue::FeedMessages(Logger* logger, uint32_t maxCount)
{
	uint32_t fedCount = 0;
	while((mMessageQueue.size() > 0) && fedCount < maxCount)
	{
		std::string msg = mMessageQueue.back();
		mMessageQueue.pop_back();

		logger->LogMessage(msg);
		fedCount++;
	}
}