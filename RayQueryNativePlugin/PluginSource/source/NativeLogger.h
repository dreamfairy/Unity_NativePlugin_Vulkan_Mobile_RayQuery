#pragma once

#include <cstdio>
#include <vector>
#include "PlatformBase.h"

#define ANDROID_LOG_TAG "MobileRT"
#ifdef UNITY_ANDROID
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, ANDROID_LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...)
#endif

typedef void(*LoggerFuncPtr)(int level, const char*);
typedef void(*LoggerIntFuncPtr)(int level, int);

class NativeLogger
{
public:
	enum Level
	{
		Info = 0,
		Warning = 1,
		Error = 2
	};

	static void SetLogger(LoggerFuncPtr logger);
	static void SetIntLogger(LoggerIntFuncPtr logger);

	static void Log(Level level, const char* msg);
	static void LogInfo(const char* msg);
	static void LogWarn(const char* msg);
	static void LogError(const char* msg);

	static void LogIntInfo(int msg);
	static void LogIntWarn(int msg);
	static void LogIntError(int msg);

	static void LogInfoFormat(const char* msg, ...);

	static void Flush();

protected:
	static LoggerFuncPtr Logger;
	static LoggerIntFuncPtr IntLogger;
	static std::vector<const char*> cachedMsg;
	static std::vector<int> cachedIntMsg;
};
