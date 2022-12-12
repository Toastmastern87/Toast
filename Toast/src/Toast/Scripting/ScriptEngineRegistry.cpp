#include "tpch.h"
//#include "ScriptEngineRegistry.h"
//
//#include <mono/jit/jit.h>
//#include <mono/metadata/assembly.h>
//
//#include "Toast/Scene/Entity.h"
//
//#include "Toast/Script/ScriptWrappers.h"
//
//namespace Toast {
//
//	std::unordered_map<MonoType*, std::function<bool(Entity&)>> sHasComponentFunctions;
//	std::unordered_map<MonoType*, std::function<void(Entity&)>> sCreateComponentFunctions;
//
//	extern MonoImage* sCoreAssemblyImage;
//
//#define RegisterComponent(Type)																				\
//	{																										\
//		MonoType* type = mono_reflection_type_from_name("Toast." #Type, sCoreAssemblyImage);				\
//		if (type)																							\
//		{																									\
//			uint32_t id = mono_type_get_type(type);															\
//			sHasComponentFunctions[type] = [](Entity& entity) { return entity.HasComponent<Type>(); };		\
//			sCreateComponentFunctions[type] = [](Entity& entity) { entity.AddComponent<Type>(); };			\
//		}																									\
//		else																								\
//		{																									\
//			TOAST_CORE_ERROR("No C# component class found for " #Type "!");									\
//		}																									\
//	}																										\
//
//	void ScriptEngineRegistry::RegisterAll()
//	{
//		//Entity
//		mono_add_internal_call("Toast.Entity::CreateComponent_Native", Toast::Script::Toast_Entity_CreateComponent);
//		mono_add_internal_call("Toast.Entity::FindEntityByTag_Native", Toast::Script::Toast_Entity_FindEntityByTag);
//	}
//
//}