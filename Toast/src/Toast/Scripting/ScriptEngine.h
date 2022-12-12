 #pragma once

#include <string>
//#include "Toast/Core/Base.h"
//#include "Toast/Core/UUID.h"
//
//#include <string>
//
#include "Toast/Scene/Scene.h"
//#include "Toast/Scene/Components.h"
#include "Toast/Scene/Entity.h"
//
//extern "C" {
//	typedef struct _MonoObject MonoObject;
//	typedef struct _MonoClassField MonoClassField;
//}

extern "C" {
	typedef struct _MonoImage MonoImage;
	typedef struct _MonoClass MonoClass;
	typedef struct _MonoMethod MonoMethod;
	typedef struct _MonoObject MonoObject;
	typedef struct _MonoAssembly MonoAssembly;
}

namespace Toast {

	//enum class PropertyType 
	//{
	//	None = 0, Float
	//};

	//struct EntityScriptClass;
	//struct EntityInstance
	//{
	//	EntityScriptClass* ScriptClass = nullptr;

	//	uint32_t Handle = 0;
	//	Scene* SceneInstance = nullptr;

	//	MonoObject* GetInstance();
	//};

	//struct PublicProperty 
	//{
	//	std::string Name;
	//	PropertyType Type;
	//	
	//	PublicProperty(const std::string& name, PropertyType type);
	//	PublicProperty(const PublicProperty&) = delete;
	//	PublicProperty(PublicProperty&& other);
	//	~PublicProperty();

	//	void CopyStoredValueToRuntime();
	//	bool IsRuntimeAvailable() const;

	//	template<typename T>
	//	void SetStoredValue(T value) const
	//	{
	//		SetStoredValue_Internal(&value);
	//	}

	//	template<typename T>
	//	T GetStoredValue() const 
	//	{
	//		T value;
	//		GetStoredValue_Internal(&value);
	//		return value;
	//	}

	//	template<typename T>
	//	void SetRuntimeValue(T value) const
	//	{
	//		SetRuntimeValue_Internal(&value);
	//	}

	//	template<typename T>
	//	T GetRuntimeValue() const
	//	{
	//		T value;
	//		GetRuntimeValue_Internal(&value);
	//		return value;
	//	}

	//	void SetStoredValueRaw(void* src);
	//private:
	//	EntityInstance* mEntityInstance;
	//	MonoClassField* mMonoClassField;
	//	uint8_t* mStoredValueBuffer = nullptr;

	//	uint8_t* AllocateBuffer(PropertyType type);
	//	void SetStoredValue_Internal(void* value) const;
	//	void GetStoredValue_Internal(void* outValue) const;
	//	void SetRuntimeValue_Internal(void* value) const;
	//	void GetRuntimeValue_Internal(void* outValue) const;

	//	friend class ScriptEngine;
	//};

	//using ScriptModulePropertyMap = std::unordered_map<std::string, std::unordered_map<std::string, PublicProperty>>;

	//struct EntityInstanceData
	//{
	//	EntityInstance Instance;
	//	ScriptModulePropertyMap ModulePropertyMap;
	//};

	//using EntityInstanceMap = std::unordered_map<UUID, std::unordered_map<UUID, EntityInstanceData>>;

	class ScriptClass
	{
	public:
		ScriptClass() = default;
		ScriptClass(const std::string& classNamespace, const std::string& className);

		MonoObject* Instantiate();
		MonoMethod* GetMethod(const std::string& name, int parameterCount);
		MonoObject* InvokeMethod(MonoObject* instance, MonoMethod* method, void** params = nullptr);
	private:
		std::string mClassNamespace;
		std::string mClassName;

		MonoClass* mMonoClass = nullptr;
	};

	class ScriptInstance
	{
	public:
		ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity);

		void InvokeOnCreate();
		void InvokeOnUpdate(float ts);
		void InvokeOnEvent();
	private:
		Ref<ScriptClass> mScriptClass;

		MonoObject* mInstance = nullptr;
		MonoMethod* mConstructor = nullptr;
		MonoMethod* mOnCreateMethod = nullptr;
		MonoMethod* mOnUpdateMethod = nullptr;
		MonoMethod* mOnEventMethod = nullptr;
	};

	class ScriptEngine 
	{
	public:
		static void Init();
		static void Shutdown();

		static void LoadAssembly(const std::string& path);

		static void OnRuntimeStart(Scene* scene);
		static void OnRuntimeStop();

		static bool EntityClassExists(const std::string& fullClassName);
		static void OnCreateEntity(Entity entity);
		static void OnUpdateEntity(Entity entity, Timestep ts);
		static void OnEventEntity(Entity entity);

		static Scene* GetSceneContext();
		static std::unordered_map<std::string, Ref<ScriptClass>> GetEntityClasses();

		static MonoImage* GetCoreAssemblyImage();

		//static void LoadToastRuntimeAssembly(const std::string& path);
		//static void ReloadAssembly(const std::string& path);

		//static void SetSceneContext(const Ref<Scene>& scene);
		//static const Ref<Scene>& GetCurrentSceneContext();

		//static void CopyEntityScriptData(UUID dst, UUID src);

		//static void OnCreateEntity(Entity entity);
		//static void OnUpdateEntity(UUID sceneID, UUID entityID, Timestep ts);
		//static void OnClickEntity(Entity entity);

		//static bool ModuleExists(const std::string& moduleName);
		//static void InitScriptEntity(Entity entity);
		//static void ShutdownScriptEntity(UUID sceneID, UUID entityID, const std::string& moduleName);
		//static void InstantiateEntityClass(Entity entity);

		//static EntityInstanceMap& GetEntityInstanceMap();
		//static EntityInstanceData& GetEntityInstanceData(UUID sceneID, UUID entityID);
	private:
		static void InitMono();
		static void ShutdownMono();

		static MonoObject* InstantiateClass(MonoClass* monoClass);
		static void LoadAssemblyClasses(MonoAssembly* assembly);

		friend class ScriptClass;
		friend class ScriptGlue;
	};
}