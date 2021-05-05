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
		// Input ///////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////

		bool Toast_Input_IsKeyPressed(KeyCode key)
		{
			return Input::IsKeyPressed(key);
		}

		bool Toast_Input_IsMouseButtonPressed(MouseCode button)
		{
			return Input::IsMouseButtonPressed(button);
		}

		void Toast_Input_GetMousePosition(DirectX::XMFLOAT2* outPos)
		{
			*outPos = Input::GetMousePosition();
		}

		float Toast_Input_GetMouseWheelDelta()
		{
			return Input::GetMouseWheelDelta();
		}

		void Toast_Input_SetMouseWheelDelta(float value)
		{
			Input::SetMouseWheelDelta(value);
		}

		////////////////////////////////////////////////////////////////
		// Entity //////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////

		void Toast_Entity_CreateComponent(uint64_t entityID, void* type)
		{
			Ref<Scene> scene = ScriptEngine::GetCurrentSceneContext();
			TOAST_CORE_ASSERT(scene, "No active scene!");
			const auto& entityMap = scene->GetEntityMap();
			TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");

			Entity entity = entityMap.at(entityID);	
			MonoType* monotype = mono_reflection_type_get_type((MonoReflectionType*)type);
			sCreateComponentFunctions[monotype](entity);
		}

		bool Toast_Entity_HasComponent(uint64_t entityID, void* type)
		{
			Ref<Scene> scene = ScriptEngine::GetCurrentSceneContext();
			TOAST_CORE_ASSERT(scene, "No active scene!");
			const auto& entityMap = scene->GetEntityMap();
			TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
			Entity entity = entityMap.at(entityID);

			MonoType* monoType = mono_reflection_type_get_type((MonoReflectionType*)type);
			return sHasComponentFunctions[monoType](entity);
		}

		uint64_t Toast_Entity_FindEntityByTag(MonoString* tag)
		{
			Ref<Scene> scene = ScriptEngine::GetCurrentSceneContext();
			TOAST_CORE_ASSERT(scene, "No active scene!");

			Entity entity = scene->FindEntityByTag(mono_string_to_utf8(tag));
			if (entity)
				return entity.GetComponent<IDComponent>().ID;

			return 0;
		}

		////////////////////////////////////////////////////////////////
		// Tag /////////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////

		MonoString* Toast_TagComponent_GetTag(uint64_t entityID)
		{
			Ref<Scene> scene = ScriptEngine::GetCurrentSceneContext();
			TOAST_CORE_ASSERT(scene, "No active scene!");
			const auto& entityMap = scene->GetEntityMap();
			TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
			Entity entity = entityMap.at(entityID);
			auto& component = entity.GetComponent<TagComponent>();
			std::string tag = component.Tag;

			return ConvertCppStringToMonoString(mono_domain_get(), tag);
		}

		void Toast_TagComponent_SetTag(uint64_t entityID, MonoString* tag)
		{
			Ref<Scene> scene = ScriptEngine::GetCurrentSceneContext();
			TOAST_CORE_ASSERT(scene, "No active scene!");
			const auto& entityMap = scene->GetEntityMap();
			TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
			Entity entity = entityMap.at(entityID);
			auto& component = entity.GetComponent<TagComponent>();
			std::string& tagStr = ConvertMonoStringToCppString(tag);
			component.Tag = tagStr;
		}

		////////////////////////////////////////////////////////////////
		// Transform ///////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////

		void Toast_TransformComponent_GetTransform(uint64_t entityID, DirectX::XMMATRIX* outTransform)
		{
			Ref<Scene> scene = ScriptEngine::GetCurrentSceneContext();
			TOAST_CORE_ASSERT(scene, "No active scene!");
			const auto& entityMap = scene->GetEntityMap();
			TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
			Entity entity = entityMap.at(entityID);
			auto& tc = entity.GetComponent<TransformComponent>();
			*outTransform = tc.Transform;
		}

		void Toast_TransformComponent_SetTransform(uint64_t entityID, DirectX::XMMATRIX* inTransform)
		{
			Ref<Scene> scene = ScriptEngine::GetCurrentSceneContext();
			TOAST_CORE_ASSERT(scene, "No active scene!");
			const auto& entityMap = scene->GetEntityMap();
			TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
			Entity entity = entityMap.at(entityID);
			auto& tc = entity.GetComponent<TransformComponent>();
			tc.Transform = *inTransform;
		}

		void Toast_TransformComponent_GetTranslation(uint64_t entityID, DirectX::XMFLOAT3* outTranslation)
		{
			//Ref<Scene> scene = ScriptEngine::GetCurrentSceneContext();
			//TOAST_CORE_ASSERT(scene, "No active scene!");
			//const auto& entityMap = scene->GetEntityMap();
			//TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in scene!");
			//Entity entity = entityMap.at(entityID);
			//auto& component = entity.GetComponent<TransformComponent>();
			//*outTranslation = component.Translation;
		}

		void Toast_TransformComponent_SetTranslation(uint64_t entityID, DirectX::XMFLOAT3* inTranslation)
		{
			//Ref<Scene> scene = ScriptEngine::GetCurrentSceneContext();
			//TOAST_CORE_ASSERT(scene, "No active scene!");
			//const auto& entityMap = scene->GetEntityMap();
			//TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in scene!");
			//Entity entity = entityMap.at(entityID);
			//auto& component = entity.GetComponent<TransformComponent>();
			//component.Translation = *inTranslation;
		}

		void Toast_TransformComponent_GetRotation(uint64_t entityID, DirectX::XMFLOAT3* outRotation)
		{
			//Ref<Scene> scene = ScriptEngine::GetCurrentSceneContext();
			//TOAST_CORE_ASSERT(scene, "No active scene!");
			//const auto& entityMap = scene->GetEntityMap();
			//TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
			//Entity entity = entityMap.at(entityID);
			//auto& component = entity.GetComponent<TransformComponent>();
			//*outRotation = component.Rotation;
		}

		void Toast_TransformComponent_SetRotation(uint64_t entityID, DirectX::XMFLOAT3* inRotation)
		{
			//Ref<Scene> scene = ScriptEngine::GetCurrentSceneContext();
			//TOAST_CORE_ASSERT(scene, "No active scene!");
			//const auto& entityMap = scene->GetEntityMap();
			//TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
			//Entity entity = entityMap.at(entityID);
			//auto& component = entity.GetComponent<TransformComponent>();

			//component.Rotation = *inRotation;
		}

		void Toast_TransformComponent_GetScale(uint64_t entityID, DirectX::XMFLOAT3* outScale)
		{
			//Ref<Scene> scene = ScriptEngine::GetCurrentSceneContext();
			//TOAST_CORE_ASSERT(scene, "No active scene!");
			//const auto& entityMap = scene->GetEntityMap();
			//TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
			//Entity entity = entityMap.at(entityID);
			//auto& component = entity.GetComponent<TransformComponent>();
			//*outScale = component.Scale;
		}

		void Toast_TransformComponent_SetScale(uint64_t entityID, DirectX::XMFLOAT3* inScale)
		{
			//Ref<Scene> scene = ScriptEngine::GetCurrentSceneContext();
			//TOAST_CORE_ASSERT(scene, "No active scene!");
			//const auto& entityMap = scene->GetEntityMap();
			//TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
			//Entity entity = entityMap.at(entityID);
			//auto& component = entity.GetComponent<TransformComponent>();

			//component.Scale = *inScale;
		}

		////////////////////////////////////////////////////////////////
		// Planet //////////////////////////////////////////////////////
		////////////////////////////////////////////////////////////////

		void Toast_PlanetComponent_GetRadius(uint64_t entityID, float* outRadius)
		{
			Ref<Scene> scene = ScriptEngine::GetCurrentSceneContext();
			TOAST_CORE_ASSERT(scene, "No active scene!");
			const auto& entityMap = scene->GetEntityMap();
			TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in the scene!");
			Entity entity = entityMap.at(entityID);
			auto& component = entity.GetComponent<PlanetComponent>();

			*outRadius = component.PlanetData.radius.x;
		}

	}
}