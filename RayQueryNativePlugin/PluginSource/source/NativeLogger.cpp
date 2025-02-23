#include "NativeLogger.h"
#include <stdarg.h>

LoggerFuncPtr NativeLogger::Logger = nullptr;
LoggerIntFuncPtr NativeLogger::IntLogger = nullptr;
std::vector<const char*> NativeLogger::cachedMsg;
std::vector<int> NativeLogger::cachedIntMsg;


void NativeLogger::SetLogger(LoggerFuncPtr logger)
{
	Logger = logger;
	LogInfo("SetLog Done");
}

void NativeLogger::SetIntLogger(LoggerIntFuncPtr logger)
{
	IntLogger = logger;
	LogInfo("SetIntLog Done");
}

void NativeLogger::Log(NativeLogger::Level level, const char* msg)
{
	if (level == Level::Info)
		LogInfo(msg);
	else if (level == Level::Warning)
		LogWarn(msg);
	else if (level == Level::Error)
		LogError(msg);
}

void NativeLogger::LogInfo(const char* msg)
{
	LOGI("%s",msg);

	if (nullptr != Logger)
	{
		Logger((int)Level::Info, msg);
	}
	else {
		//char* tmp = strdup(msg);
		//cachedMsg.push_back(tmp);
	}
}

void NativeLogger::LogWarn(const char* msg)
{
	LOGI("%s", msg);

	if (nullptr != Logger)
	{
		Logger((int)Level::Warning, msg);
	}
	else {
		//cachedMsg.push_back(msg);
	}
}

void NativeLogger::LogError(const char* msg)
{
	LOGI("%s", msg);

	if (nullptr != Logger)
	{
		Logger((int)Level::Error, msg);
	}
	else {
		//cachedMsg.push_back(msg);
	}
}

void NativeLogger::LogIntInfo(int msg)
{
	LOGI("%d", msg);

	if (nullptr != IntLogger)
	{
		IntLogger((int)Level::Info, msg);
	}
	else {
		//cachedIntMsg.push_back(msg);
	}
}

void NativeLogger::LogIntWarn(int msg)
{
	LOGI("%d", msg);

	if (nullptr != IntLogger)
	{
		IntLogger((int)Level::Warning, msg);
	}
	else {
		//cachedIntMsg.push_back(msg);
	}
}

void NativeLogger::LogIntError(int msg)
{
	LOGI("%d", msg);

	if (nullptr != IntLogger)
	{
		IntLogger((int)Level::Error, msg);
	}
	else {
		//cachedIntMsg.push_back(msg);
	}
}

void NativeLogger::LogInfoFormat(const char* msg, ...)
{
	char buffer[1024];
	va_list args;
	va_start(args, msg);

	vsnprintf(buffer, sizeof(buffer), msg, args);
	va_end(args);

	LogInfo(buffer);
}

void NativeLogger::Flush()
{
	/*if (NULL != Logger && !cachedMsg.empty())
	{
		for (auto itor = cachedMsg.begin(); itor != cachedMsg.end(); ++itor)
		{
			LogInfo(*itor);
		}
		cachedMsg.clear();
	}

	if (NULL != IntLogger && !cachedIntMsg.empty())
	{
		for (auto itor = cachedIntMsg.begin(); itor != cachedIntMsg.end(); ++itor)
		{
			LogIntInfo(*itor);
		}
		cachedIntMsg.clear();
	}*/
}
