#include "tpch.h"
#include "ScriptWrappers.h"

#include "Toast/Script/ScriptEngine.h"
#include "Toast/Script/MonoUtils.h"

namespace Toast {

	namespace Script {

		void Toast_Console_LogTrace(MonoObject* message)
		{
			TOAST_TRACE(ConvertMonoObjectToString(message));
		}

		void Toast_Console_LogInfo(MonoObject* message)
		{
			TOAST_INFO(ConvertMonoObjectToString(message));
		}

		void Toast_Console_LogWarning(MonoObject* message)
		{
			TOAST_WARN(ConvertMonoObjectToString(message));
		}

		void Toast_Console_LogError(MonoObject* message)
		{
			TOAST_ERROR(ConvertMonoObjectToString(message));
		}

		void Toast_Console_LogCritical(MonoObject* message)
		{
			TOAST_CRITICAL(ConvertMonoObjectToString(message));
		}
	}
}