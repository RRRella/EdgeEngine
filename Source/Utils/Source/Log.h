#pragma once

#include <string>

#define VARIADIC_LOG_FN(FN_NAME)\
template<class... Args>\
void FN_NAME(const char* format, Args&&... args)\
{\
	char msg[LEN_MSG_BUFFER];\
	sprintf_s(msg, format, args...);\
	FN_NAME(std::string(msg));\
}

namespace Log
{
	constexpr size_t LEN_MSG_BUFFER = 4096;
	struct LogInitializeParams 
	{
		bool bLogConsole        = false;
		bool bLogFile           = false;
		std::string LogFilePath = "./";
	};

	void Initialize(const LogInitializeParams& params);
	void Destroy();

	void Info(const std::string& s);
	void Error(const std::string& s);
	void Warning(const std::string& s);
	
	VARIADIC_LOG_FN(Error)
	VARIADIC_LOG_FN(Warning)
	VARIADIC_LOG_FN(Info)
}