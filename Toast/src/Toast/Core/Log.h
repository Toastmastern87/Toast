#pragma once

#include "Toast/Core/Base.h"

#include <string>
#include <vector>

namespace Toast{

	enum class Severity 
	{
		Trace,
		Info,
		Warning,
		Error,
		Critical
	};

	class Log
	{
	public:
		Log(const char* name);

		void LogMsg(Severity severity, const char* format, ...);
		void LogTrace(const char* format, ...);
		void LogInfo(const char* format, ...);
		void LogWarning(const char* format, ...);
		void LogError(const char* format, ...);
		void LogCritical(const char* format, ...);
		static Ref<Log>& GetCoreLogger() { return sCoreLogger; }
		static Ref<Log>& GetClientLogger() { return sClientLogger; }

		static void Init();
		static void Shutdown();
		static void Flush();
	private:
		static const char* GetSeverityID(Severity severity);
		static const char* GetSeverityConsoleColor(Severity severity);
		static uint32_t GetSeverityMaxBufferCount(Severity severity);
		static void LogMsg(const char* name, Severity severity, const char* format, va_list args);
	private:
		const char* mName;

		static Ref<Log> sCoreLogger;
		static Ref<Log> sClientLogger;
		static std::vector<std::string> sBuffer;

		static std::vector<std::pair<Severity, std::string>> sMessages;
		
		static bool sLogToFile;
		static bool sLogToConsole;
		static bool sLogToToasterConsole;

		static const char* sPreviousFile;
		static const char* sCurrentFile;

		friend class ConsolePanel;
	};
}

/* Formatters
    %c                  Character
	%d                  Signed integer
	%e or %E            Scientific notation of floats
	%f                  Float values
	%g or %G            Similar as %e or %E
	%hi                 Signed integer (short)
	%hu                 Unsigned Integer (short)
	%i                  Unsigned integer
	%l or %ld or %li    Long
	%lf                 Double
	%Lf                 Long double
	%lu                 Unsigned int or unsigned long
	%lli or %lld        Long long
	%llu                Unsigned long long
	%o                  Octal representation
	%p                  Pointer
	%s                  String
	%u                  Unsigned int
	%x or %X            Hexadecimal representation
	%n                  Prints nothing
	%%                  Prints % character
 */

// Core log macros
#define TOAST_CORE_TRACE(...)     ::Toast::Log::GetCoreLogger()->LogTrace(__VA_ARGS__)
#define TOAST_CORE_INFO(...)      ::Toast::Log::GetCoreLogger()->LogInfo(__VA_ARGS__)
#define TOAST_CORE_WARN(...)      ::Toast::Log::GetCoreLogger()->LogWarning(__VA_ARGS__)
#define TOAST_CORE_ERROR(...)     ::Toast::Log::GetCoreLogger()->LogError(__VA_ARGS__)
#define TOAST_CORE_CRITICAL(...)  ::Toast::Log::GetCoreLogger()->LogCritical(__VA_ARGS__)

// Client log macros
#define TOAST_TRACE(...)          ::Toast::Log::GetClientLogger()->LogTrace(__VA_ARGS__)
#define TOAST_INFO(...)           ::Toast::Log::GetClientLogger()->LogInfo(__VA_ARGS__)
#define TOAST_WARN(...)           ::Toast::Log::GetClientLogger()->LogWarning(__VA_ARGS__)
#define TOAST_ERROR(...)          ::Toast::Log::GetClientLogger()->LogError(__VA_ARGS__)
#define TOAST_CRITICAL(...)       ::Toast::Log::GetClientLogger()->LogCritical(__VA_ARGS__)