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

	using EntityInstanceMap = std::unordered_map<uint32_t, std::unordered_map<uint32_t, EntityInstanceData>>;

	class ScriptEngine 
	{
	public:
		static void Init(const std::string& assemblyPath);
		static void Shutdown();

		static void LoadToastRuntimeAssembly(const std::string& path);

		static bool ModuleExists(const std::string& moduleName);
		static void InitScriptEntity(Entity entity);
	};
}