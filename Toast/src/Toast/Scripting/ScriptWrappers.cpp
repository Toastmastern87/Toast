#include "tpch.h"
//#include "ScriptWrappers.h"
//
//#include "Toast/Renderer/Mesh.h"
//
//#include "Toast/Script/ScriptEngine.h"
//#include "Toast/Script/MonoUtils.h"
//
//#include "mono/metadata/appdomain.h"
//
//namespace Toast {
//
//	extern std::unordered_map<MonoType*, std::function<bool(Entity&)>> sHasComponentFunctions;
//	extern std::unordered_map<MonoType*, std::function<void(Entity&)>> sCreateComponentFunctions;
//
//}
//
//namespace Toast {
//
//	namespace Script {
//
//
//		////////////////////////////////////////////////////////////////
//		// Entity //////////////////////////////////////////////////////
//		////////////////////////////////////////////////////////////////
//
//		void Toast_Entity_CreateComponent(uint64_t entityID, void* type)
//		{
//			Ref<Scene> scene = ScriptEngine::GetCurrentSceneContext();
//			TOAST_CORE_ASSERT(scene, "No active scene!");
//			const auto& entityMap = scene->GetEntityMap();
//			TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
//
//			Entity entity = entityMap.at(entityID);	
//			MonoType* monotype = mono_reflection_type_get_type((MonoReflectionType*)type);
//			sCreateComponentFunctions[monotype](entity);
//		}
//
//		uint64_t Toast_Entity_FindEntityByTag(MonoString* tag)
//		{
//			Ref<Scene> scene = ScriptEngine::GetCurrentSceneContext();
//			TOAST_CORE_ASSERT(scene, "No active scene!");
//
//			Entity entity = scene->FindEntityByTag(mono_string_to_utf8(tag));
//			if (entity)
//				return entity.GetComponent<IDComponent>().ID;
//
//			return 0;
//		}
// 
//	}
// 
//}