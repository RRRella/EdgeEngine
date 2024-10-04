#pragma once

#include <string>

namespace Settings { struct Logger; }




#define VARIADIC_LOG_FN(FN_NAME)\
template<class... Args>\
void FN_NAME(const char* format, Args&&... args)\
{\
	char msg[LEN_MSG_BUFFER];\
	sprintf_s(msg, format, args...);\
	##FN_NAME(std::string(msg));\
}

namespace Log
{
	enum Mode : unsigned	// unused.
	{
		NONE				= 0,	// Visual Studio Output Window
		CONSOLE				= 1,	// Separate Console Window
		FILE				= 2,	// Log File in %APPDATA%\Engine\DATE-EngineLog.txt
		
		CONSOLE_AND_FILE	= CONSOLE | FILE,	// Both Console Window & Log File
	};

	//---------------------------------------------------------------------------------------------

	constexpr size_t LEN_MSG_BUFFER = 4096;
	struct LogInitializeParams 
	{
		bool bLogConsole        = false;
		bool bLogFile           = false;
		std::string LogFilePath = "./";
	};

	//---------------------------------------------------------------------------------------------

	void Initialize(const LogInitializeParams& params);
	void Destroy();

	void Info(const std::string& s);
	void Error(const std::string& s);
	void Warning(const std::string& s);
	
	VARIADIC_LOG_FN(Error)
	VARIADIC_LOG_FN(Warning)
	VARIADIC_LOG_FN(Info)
}