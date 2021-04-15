#include "tpch.h"
#include "ScriptWrappers.h"

#include "Toast/Script/ScriptEngine.h"
#include "Toast/Script/MonoUtils.h"

namespace Toast {

	namespace Script {

		void Toast_Console_LogTrace(MonoObject* msg)
		{
			TOAST_TRACE(ConvertMonoObjectToCppChar(msg));
		}

		void Toast_Console_LogInfo(MonoObject* msg)
		{
			TOAST_INFO(ConvertMonoObjectToCppChar(msg));
		}

		void Toast_Console_LogWarning(MonoObject* msg)
		{
			TOAST_WARN(ConvertMonoObjectToCppChar(msg));
		}

		void Toast_Console_LogError(MonoObject* msg)
		{
			TOAST_ERROR(ConvertMonoObjectToCppChar(msg));
		}

		void Toast_Console_LogCritical(MonoObject* msg)
		{
			TOAST_CRITICAL(ConvertMonoObjectToCppChar(msg));
		}
	}
}