#pragma once

#include "Toast/Core/Base.h"
#include "Toast/Core/UUID.h"

#include <string>

#include "Toast/Scene/Components.h"
#include "Toast/Scene/Entity.h"

extern "C" {
	typedef struct _MonoObject MonoObject;
	typedef struct _MonoClassField MonoClassField;
}

namespace Toast {

	enum class PropertyType 
	{
		None = 0, Float
	};

	struct EntityScriptClass;
	struct EntityInstance
	{
		EntityScriptClass* ScriptClass = nullptr;

		uint32_t Handle = 0;
		Scene* SceneInstance = nullptr;

		MonoObject* GetInstance();
	};

	struct PublicProperty 
	{
		std::string Name;
		PropertyType Type;
		
		PublicProperty(const std::string& name, PropertyType type);
		PublicProperty(const PublicProperty&) = delete;
		PublicProperty(PublicProperty&& other);
		~PublicProperty();

		void CopyStoredValueToRuntime();
		bool IsRuntimeAvailable() const;

		template<typename T>
		void SetStoredValue(T value) const
		{
			SetStoredValue_Internal(&value);
		}

		template<typename T>
		T GetStoredValue() const 
		{
			T value;
			GetStoredValue_Internal(&value);
			return value;
		}

		template<typename T>
		void SetRuntimeValue(T value) const
		{
			SetRuntimeValue_Internal(&value);
		}

		template<typename T>
		T GetRuntimeValue() const
		{
			T value;
			GetRuntimeValue_Internal(&value);
			return value;
		}

		void SetStoredValueRaw(void* src);
	private:
		EntityInstance* mEntityInstance;
		MonoClassField* mMonoClassField;
		uint8_t* mStoredValueBuffer = nullptr;

		uint8_t* AllocateBuffer(PropertyType type);
		void SetStoredValue_Internal(void* value) const;
		void GetStoredValue_Internal(void* outValue) const;
		void SetRuntimeValue_Internal(void* value) const;
		void GetRuntimeValue_Internal(void* outValue) const;

		friend class ScriptEngine;
	};

	using ScriptModulePropertyMap = std::unordered_map<std::string, std::unordered_map<std::string, PublicProperty>>;

	struct EntityInstanceData
	{
		EntityInstance Instance;
		ScriptModulePropertyMap ModulePropertyMap;
	};

	using EntityInstanceMap = std::unordered_map<UUID, std::unordered_map<UUID, EntityInstanceData>>;

	class ScriptEngine 
	{
	public:
		static void Init(const std::string& assemblyPath);
		static void Shutdown();

		static void LoadToastRuntimeAssembly(const std::string& path);
		static void ReloadAssembly(const std::string& path);

		static void SetSceneContext(const Ref<Scene>& scene);
		static const Ref<Scene>& GetCurrentSceneContext();

		static void CopyEntityScriptData(UUID dst, UUID src);

		static void OnCreateEntity(Entity entity);
		static void OnUpdateEntity(UUID sceneID, UUID entityID, Timestep ts);
		static void OnClickEntity(Entity entity);

		static bool ModuleExists(const std::string& moduleName);
		static void InitScriptEntity(Entity entity);
		static void ShutdownScriptEntity(UUID sceneID, UUID entityID, const std::string& moduleName);
		static void InstantiateEntityClass(Entity entity);

		static EntityInstanceMap& GetEntityInstanceMap();
		static EntityInstanceData& GetEntityInstanceData(UUID sceneID, UUID entityID);
	};
}