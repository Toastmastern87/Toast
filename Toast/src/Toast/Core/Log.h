#pragma once

#include "Toast/Core/Core.h"

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

namespace Toast 
{
	class Log
	{
	public:
		static void Init();

		inline static Ref<spdlog::logger>& GetCoreLogger() { return sCoreLogger;  }
		inline static Ref<spdlog::logger>& GetClientLogger() { return sClientLogger; }
	private:
		static Ref<spdlog::logger> sCoreLogger;
		static Ref<spdlog::logger> sClientLogger;
	};
}

// Core log macros
#define TOAST_CORE_TRACE(...)     ::Toast::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define TOAST_CORE_INFO(...)      ::Toast::Log::GetCoreLogger()->info(__VA_ARGS__)
#define TOAST_CORE_WARN(...)      ::Toast::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define TOAST_CORE_ERROR(...)     ::Toast::Log::GetCoreLogger()->error(__VA_ARGS__)
#define TOAST_CORE_CRITICAL(...)  ::Toast::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Client log macros
#define TOAST_TRACE(...)          ::Toast::Log::GetClientLogger()->trace(__VA_ARGS__)
#define TOAST_INFO(...)           ::Toast::Log::GetClientLogger()->info(__VA_ARGS__)
#define TOAST_WARN(...)           ::Toast::Log::GetClientLogger()->warn(__VA_ARGS__)
#define TOAST_ERROR(...)          ::Toast::Log::GetClientLogger()->error(__VA_ARGS__)
#define TOAST_CRITICAL(...)       ::Toast::Log::GetClientLogger()->critical(__VA_ARGS__)