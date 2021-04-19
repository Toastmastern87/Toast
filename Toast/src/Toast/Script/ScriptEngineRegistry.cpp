#include "tpch.h"
#include "ScriptEngineRegistry.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>

#include "Toast/Scene/Entity.h"

#include "Toast/Script/ScriptWrappers.h"

namespace Toast {

	std::unordered_map<MonoType*, std::function<bool(Entity&)>> sHasComponentFunctions;
	std::unordered_map<MonoType*, std::function<void(Entity&)>> sCreateComponentFunctions;

	extern MonoImage* sCoreAssemblyImage;

#define RegisterComponent(Type)																				\
	{																										\
		MonoType* type = mono_reflection_type_from_name("Toast." #Type, sCoreAssemblyImage);				\
		if (type)																							\
		{																									\
			uint32_t id = mono_type_get_type(type);															\
			sHasComponentFunctions[type] = [](Entity& entity) { return entity.HasComponent<Type>(); };		\
			sCreateComponentFunctions[type] = [](Entity& entity) { entity.AddComponent<Type>(); };			\
		}																									\
		else																								\
		{																									\
			TOAST_CORE_ERROR("No C# component class found for " #Type "!");									\
		}																									\
	}																										\

	void ScriptEngineRegistry::RegisterAll()
	{
		sHasComponentFunctions.clear();
		sCreateComponentFunctions.clear();
		RegisterComponent(TagComponent);

		//Log
		mono_add_internal_call("Toast.Console::LogTrace_Native", Toast::Script::Toast_Console_LogTrace);
		mono_add_internal_call("Toast.Console::LogInfo_Native", Toast::Script::Toast_Console_LogInfo);
		mono_add_internal_call("Toast.Console::LogWarning_Native", Toast::Script::Toast_Console_LogWarning);
		mono_add_internal_call("Toast.Console::LogError_Native", Toast::Script::Toast_Console_LogError);
		mono_add_internal_call("Toast.Console::LogCritical_Native", Toast::Script::Toast_Console_LogCritical);

		//Entity
		mono_add_internal_call("Toast.Entity::CreateComponent_Native", Toast::Script::Toast_Entity_CreateComponent);
		mono_add_internal_call("Toast.Entity::HasComponent_Native", Toast::Script::Toast_Entity_HasComponent);
		mono_add_internal_call("Toast.Entity::FindEntityByTag_Native", Toast::Script::Toast_Entity_FindEntityByTag);

		//Tag
		mono_add_internal_call("Toast.TagComponent::GetTag_Native", Toast::Script::Toast_TagComponent_GetTag);
		mono_add_internal_call("Toast.TagComponent::SetTag_Native", Toast::Script::Toast_TagComponent_SetTag);
	}

}