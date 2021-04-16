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

		// Entity
		void Toast_Entity_CreateComponent(uint32_t entityID, void* type);
		bool Toast_Entity_HasComponent(uint32_t entityID, void* type);
		uint32_t Toast_Entity_FindEntityByTag(MonoString* tag);

		// Tag Component
		MonoString* Toast_TagComponent_GetTag(uint32_t entityID);
		void Toast_TagComponent_SetTag();

	}

}