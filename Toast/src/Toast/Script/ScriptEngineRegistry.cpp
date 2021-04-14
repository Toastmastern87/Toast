#include "tpch.h"
#include "ScriptEngineRegistry.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>

#include "Toast/Script/ScriptWrappers.h"

namespace Toast {

	void ScriptEngineRegistry::RegisterAll()
	{
		mono_add_internal_call("Toast.Console::LogTrace_Native", Toast::Script::Toast_Console_LogTrace);
		mono_add_internal_call("Toast.Console::LogInfo_Native", Toast::Script::Toast_Console_LogInfo);
		mono_add_internal_call("Toast.Console::LogWarning_Native", Toast::Script::Toast_Console_LogWarning);
		mono_add_internal_call("Toast.Console::LogError_Native", Toast::Script::Toast_Console_LogError);
		mono_add_internal_call("Toast.Console::LogCritical_Native", Toast::Script::Toast_Console_LogCritical);
	}

}