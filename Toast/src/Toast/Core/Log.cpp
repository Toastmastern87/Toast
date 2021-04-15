#include "tpch.h"
#include "Log.h"

#include <filesystem>
#include <stdarg.h>
#include <stdio.h>

namespace Toast {

	Ref<Log> Log::sCoreLogger;
	Ref<Log> Log::sClientLogger;
	std::vector<std::string> Log::sBuffer;
	std::vector<std::pair<Severity, std::string>> Log::sMessages;

	bool Log::sLogToFile = true;
	bool Log::sLogToConsole = true;
	bool Log::sLogToToasterConsole = true;

	const char* Log::sPreviousFile = "Logs/ToastPrevLog.tlog";
	const char* Log::sCurrentFile = "Logs/ToastLog.tlog";

	Log::Log(const char* name)
		: mName(name) 
	{
	}

	void Log::LogMsg(Severity severity, const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		Log::LogMsg(mName, severity, format, args);
		va_end(args);
	}

	void Log::LogTrace(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		Log::LogMsg(mName, Severity::Trace, format, args);
		va_end(args);
	}

	void Log::LogInfo(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		Log::LogMsg(mName, Severity::Info, format, args);
		va_end(args);
	}

	void Log::LogWarning(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		Log::LogMsg(mName, Severity::Warning, format, args);
		va_end(args);
	}

	void Log::LogError(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		Log::LogMsg(mName, Severity::Error, format, args);
		va_end(args);
	}

	void Log::LogCritical(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		Log::LogMsg(mName, Severity::Critical, format, args);
		va_end(args);
	}

	void Log::Init()
	{
		sCoreLogger = CreateRef<Log>("Toast");
		sClientLogger = CreateRef<Log>("App");

		if (std::filesystem::exists(Log::sCurrentFile))
		{
			if (std::filesystem::exists(Log::sPreviousFile))
				std::filesystem::remove(Log::sPreviousFile);

			if (rename(Log::sCurrentFile, Log::sPreviousFile))
				Log("Logger").LogMsg(Severity::Warning, "Failed to rename log file %s to %s", Log::sCurrentFile, Log::sPreviousFile);
		}
	}

	void Log::LogMsg(const char* name, Severity severity, const char* format, va_list args)
	{
		uint32_t length = vsnprintf(nullptr, 0, format, args) + 1;
		char* buf = new char[length];
		vsnprintf(buf, length, format, args);

		std::string message(buf);
		delete[] buf;

		std::vector<std::string> messages;

		uint32_t lastIndex = 0;
		for (uint32_t i = 0; i < message.length(); i++)
		{
			if (message[i] == '\n')
			{
				messages.push_back(message.substr(lastIndex, i - lastIndex));
				lastIndex = i + 1;
			}
			else if (i == message.length() - 1)
				messages.push_back(message.substr(lastIndex));
		}

		for (std::string msg : messages)
		{
			std::string logMsg = "";
			std::string systemConsoleMsg = "";
			std::string editorConsoleMsg = "";

			constexpr uint32_t timeBufferSize = 16;
			std::time_t    currentTime = std::time(nullptr);
			char           timeBuffer[timeBufferSize];

			if (Log::sLogToFile)
				logMsg += "[" + std::string(name) + "]";
			if (Log::sLogToConsole)
				systemConsoleMsg += std::string(Log::GetSeverityConsoleColor(severity)) + "[" + std::string(name) + "]";

			if (std::strftime(timeBuffer, timeBufferSize, "[%H:%M:%S]", std::localtime(&currentTime)))
			{
				if (Log::sLogToFile)
					logMsg += timeBuffer;
				if (Log::sLogToConsole)
					systemConsoleMsg += timeBuffer;
				if (Log::sLogToToasterConsole)
					editorConsoleMsg += timeBuffer;
			}

			if (Log::sLogToFile)
				logMsg += " " + std::string(Log::GetSeverityID(severity)) + ": " + msg + "\n";
			if (Log::sLogToConsole)
				systemConsoleMsg += " " + std::string(Log::GetSeverityID(severity)) + ": " + msg + +"\033[0m " + "\n";

			if (Log::sLogToFile)
				Log::sBuffer.push_back(logMsg);
			if (Log::sLogToConsole)
				printf("%s", systemConsoleMsg.c_str());

			//ImGui Console
			if (Log::sLogToToasterConsole) 
			{
				editorConsoleMsg += " " + std::string(Log::GetSeverityID(severity)) + ": " + msg;
				sMessages.emplace_back(std::pair<Severity, std::string>(severity, editorConsoleMsg));
			}
			
		}

		if (Log::sLogToFile)
			if (Log::sBuffer.size() > Log::GetSeverityMaxBufferCount(severity))
				Flush();
	}

	void Log::Shutdown()
	{
		Flush();
	}

	void Log::Flush()
	{
		if (!Log::sLogToFile)
			return;

		std::filesystem::path filepath{ Log::sCurrentFile };
		std::filesystem::create_directories(filepath.parent_path());

		FILE* file = fopen(Log::sCurrentFile, "a");
		if (file)
		{
			for (auto message : Log::sBuffer)
				fwrite(message.c_str(), sizeof(char), message.length(), file);
			fclose(file);
			Log::sBuffer.clear();
		}
		else
			Log::sLogToFile = false;
	}

	uint32_t Log::GetSeverityMaxBufferCount(Severity severity)
	{
		switch (severity)
		{
		case Severity::Trace:
			return 100;
		case Severity::Info:
			return 100;
		case Severity::Warning:
			return 10;
		case Severity::Error:
			return 0;
		case Severity::Critical:
			return 0;
		}
		return 0;
	}

	const char* Log::GetSeverityID(Severity severity)
	{
		switch (severity)
		{
		case Severity::Trace:
			return "TRACE";
		case Severity::Info:
			return "INFO";
		case Severity::Warning:
			return "WARNING";
		case Severity::Error:
			return "ERROR";
		case  Severity::Critical:
			return "CRITICAL";
		}

		return "Unknown Severity";
	}

	const char* Log::GetSeverityConsoleColor(Severity severity)
	{
		/*
		 * Console Colors https://stackoverflow.com/questions/4053837
		 * Name            FG  BG
		 * Black           30  40
		 * Red             31  41
		 * Green           32  42
		 * Yellow          33  43
		 * Blue            34  44
		 * Magenta         35  45
		 * Cyan            36  46
		 * White           37  47
		 * Bright Black    90  100
		 * Bright Red      91  101
		 * Bright Green    92  102
		 * Bright Yellow   93  103
		 * Bright Blue     94  104
		 * Bright Magenta  95  105
		 * Bright Cyan     96  106
		 * Bright White    97  107
		 */
		switch (severity)
		{
		case Severity::Trace:
			return "\033[0;97m";
		case Severity::Info:
			return "\033[0;92m";
		case Severity::Warning:
			return "\033[0;93m";
		case Severity::Error:
			return "\033[0;91m";
		case Severity::Critical:
			return "\033[0;101m";
		}
		return "\033[0;97m";
	}

}