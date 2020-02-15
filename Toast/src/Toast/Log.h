#pragma once

#include "Core.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

namespace Toast 
{
	class TOAST_API Log
	{
	public:
		static void Init();

		inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger;  }
		inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};
}

// Core log macros
#define TOAST_CORE_TRACE(...)     ::Toast::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define TOAST_CORE_INFO(...)      ::Toast::Log::GetCoreLogger()->info(__VA_ARGS__)
#define TOAST_CORE_WARN(...)      ::Toast::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define TOAST_CORE_ERROR(...)     ::Toast::Log::GetCoreLogger()->error(__VA_ARGS__)
#define TOAST_CORE_FATAL(...)     ::Toast::Log::GetCoreLogger()->fatal(__VA_ARGS__)

// Client log macros
#define TOAST_TRACE(...)          ::Toast::Log::GetClientLogger()->trace(__VA_ARGS__)
#define TOAST_INFO(...)           ::Toast::Log::GetClientLogger()->info(__VA_ARGS__)
#define TOAST_WARN(...)           ::Toast::Log::GetClientLogger()->warn(__VA_ARGS__)
#define TOAST_ERROR(...)          ::Toast::Log::GetClientLogger()->error(__VA_ARGS__)
#define TOAST_FATAL(...)          ::Toast::Log::GetClientLogger()->fatal(__VA_ARGS__)