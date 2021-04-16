#include "tpch.h"
#include "ScriptWrappers.h"

#include "Toast/Script/ScriptEngine.h"
#include "Toast/Script/MonoUtils.h"

#include "mono/metadata/appdomain.h"

namespace Toast {

	extern std::unordered_map<MonoType*, std::function<bool(Entity&)>> sHasComponentFunctions;
	extern std::unordered_map<MonoType*, std::function<void(Entity&)>> sCreateComponentFunctions;

}

namespace Toast {

	namespace Script {

		////////////////////////////////////////////////////////////////
		// Log /////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////

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

		////////////////////////////////////////////////////////////////
		// Entity //////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////

		void Toast_Entity_CreateComponent(uint32_t entityID, void* type)
		{
		}

		bool Toast_Entity_HasComponent(uint32_t entityID, void* type)
		{
			Scene* scene = ScriptEngine::GetCurrentSceneContext();
			TOAST_CORE_ASSERT(scene, "No active scene!");
			const auto& entityMap = scene->GetEntityMap();
			TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
			Entity entity = entityMap.at(entityID);

			MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);
			return sHasComponentFunctions[monoType](entity);
		}

		uint32_t Toast_Entity_FindEntityByTag(MonoString* tag)
		{
			Scene* scene = ScriptEngine::GetCurrentSceneContext();
			TOAST_CORE_ASSERT(scene, "No active scene!");

			Entity entity = scene->FindEntityByTag(mono_string_to_utf8(tag));
			if (entity)
				return (uint32_t)entity;

			return 0;

		}

		////////////////////////////////////////////////////////////////
		// Tag /////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////

		MonoString* Toast_TagComponent_GetTag(uint32_t entityID)
		{
			Scene* scene = ScriptEngine::GetCurrentSceneContext();
			TOAST_CORE_ASSERT(scene, "No active scene!");
			const auto& entityMap = scene->GetEntityMap();
			TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
			Entity entity = entityMap.at(entityID);
			auto& component = entity.GetComponent<TagComponent>();
			std::string tag = component.Tag;

			return ConvertCppStringToMonoString(mono_domain_get(), tag);
		}

		void Toast_TagComponent_SetTag()
		{
		}
	}
}