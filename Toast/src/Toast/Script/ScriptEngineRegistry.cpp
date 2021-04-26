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
		RegisterComponent(TransformComponent);
		RegisterComponent(PlanetComponent);

		//Log
		mono_add_internal_call("Toast.Console::LogTrace_Native", Toast::Script::Toast_Console_LogTrace);
		mono_add_internal_call("Toast.Console::LogInfo_Native", Toast::Script::Toast_Console_LogInfo);
		mono_add_internal_call("Toast.Console::LogWarning_Native", Toast::Script::Toast_Console_LogWarning);
		mono_add_internal_call("Toast.Console::LogError_Native", Toast::Script::Toast_Console_LogError);
		mono_add_internal_call("Toast.Console::LogCritical_Native", Toast::Script::Toast_Console_LogCritical);

		//Input 
		mono_add_internal_call("Toast.Input::IsKeyPressed_Native", Toast::Script::Toast_Input_IsKeyPressed);
		mono_add_internal_call("Toast.Input::IsMouseButtonPressed_Native", Toast::Script::Toast_Input_IsMouseButtonPressed);
		mono_add_internal_call("Toast.Input::GetMouseWheelDelta_Native", Toast::Script::Toast_Input_GetMouseWheelDelta);
		mono_add_internal_call("Toast.Input::SetMouseWheelDelta_Native", Toast::Script::Toast_Input_SetMouseWheelDelta);

		//Entity
		mono_add_internal_call("Toast.Entity::CreateComponent_Native", Toast::Script::Toast_Entity_CreateComponent);
		mono_add_internal_call("Toast.Entity::HasComponent_Native", Toast::Script::Toast_Entity_HasComponent);
		mono_add_internal_call("Toast.Entity::FindEntityByTag_Native", Toast::Script::Toast_Entity_FindEntityByTag);

		//Tag
		mono_add_internal_call("Toast.TagComponent::GetTag_Native", Toast::Script::Toast_TagComponent_GetTag);
		mono_add_internal_call("Toast.TagComponent::SetTag_Native", Toast::Script::Toast_TagComponent_SetTag);

		//Transform
		mono_add_internal_call("Toast.TransformComponent::GetTranslation_Native", Toast::Script::Toast_TransformComponent_GetTranslation);
		mono_add_internal_call("Toast.TransformComponent::SetTranslation_Native", Toast::Script::Toast_TransformComponent_SetTranslation);
		mono_add_internal_call("Toast.TransformComponent::GetRotation_Native", Toast::Script::Toast_TransformComponent_GetRotation);
		mono_add_internal_call("Toast.TransformComponent::SetRotation_Native", Toast::Script::Toast_TransformComponent_SetRotation);
		mono_add_internal_call("Toast.TransformComponent::GetScale_Native", Toast::Script::Toast_TransformComponent_GetScale);
		mono_add_internal_call("Toast.TransformComponent::SetScale_Native", Toast::Script::Toast_TransformComponent_SetScale);

		//Planet
		mono_add_internal_call("Toast.PlanetComponent::GetRadius_Native", Toast::Script::Toast_PlanetComponent_GetRadius);
	}

}