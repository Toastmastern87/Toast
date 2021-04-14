#pragma once

#include "Toast/Script/ScriptEngine.h"

extern "C" {
	typedef struct _MonoString MonoString;
	typedef struct _MonoArray MonoArray;
}

namespace Toast {

	namespace Script {

		// Console
		void Toast_Console_LogTrace(MonoObject* message);
		void Toast_Console_LogInfo(MonoObject* message);
		void Toast_Console_LogWarning(MonoObject* message);
		void Toast_Console_LogError(MonoObject* message);
		void Toast_Console_LogCritical(MonoObject* message);

	}

}