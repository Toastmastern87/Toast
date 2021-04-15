#pragma once

#include "Toast/Core/Base.h"

#include <string>

#include "Toast/Scene/Components.h"
#include "Toast/Scene/Entity.h"

extern "C" {
	typedef struct _MonoObject MonoObject;
	typedef struct _MonoClassField MonoClassField;
}

namespace Toast {

	struct EntityScriptClass;
	struct EntityInstance
	{
		EntityScriptClass* ScriptClass = nullptr;

		uint32_t Handle = 0;
		Scene* SceneInstance = nullptr;

		MonoObject* GetInstance();
	};

	struct EntityInstanceData
	{
		EntityInstance Instance;
	};

	using EntityInstanceMap = std::unordered_map<std::string, std::unordered_map<uint32_t, EntityInstanceData>>;

	class ScriptEngine 
	{
	public:
		static void Init(const std::string& assemblyPath);
		static void Shutdown();

		static void LoadToastRuntimeAssembly(const std::string& path);
		static void ReloadAssembly(const std::string& path);

		static void SetSceneContext(Scene* scene);
		static Scene* GetCurrentSceneContext();

		static void OnCreateEntity(Entity entity);
		static void OnUpdateEntity(uint32_t entityID, Timestep ts);

		static bool ModuleExists(const std::string& moduleName);
		static void InitScriptEntity(Entity entity);
		static void ShutdownScriptEntity(uint32_t entityID, const std::string& moduleName);
		static void InstantiateEntityClass(Entity entity);

		static EntityInstanceData& GetEntityInstanceData(const std::string& sceneName, uint32_t entityID);
	};
}