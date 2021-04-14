#include "tpch.h"
#include "ScriptEngine.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/attrdefs.h>

namespace Toast {

	static MonoDomain* sMonoDomain = nullptr;
	static std::string sAssemblyPath;
	static Scene* sSceneContext;

	// Assembly images
	static MonoImage* sAppAssemblyImage = nullptr;
	static MonoImage* sCoreAssemblyImage = nullptr;

	static EntityInstanceMap sEntityInstanceMap;

	static MonoAssembly* sAppAssembly = nullptr;
	static MonoAssembly* sCoreAssembly = nullptr;

	static MonoMethod* GetMethod(MonoImage* image, const std::string& methodDesc);

	struct EntityScriptClass 
	{
		std::string FullName;
		std::string ClassName;
		std::string NamespaceName;

		MonoClass* Class = nullptr;
		MonoMethod* OnCreateMethod = nullptr;

		void InitClassMethods(MonoImage* image)
		{
			OnCreateMethod = GetMethod(image, FullName + ":OnCreate()");
		}
	};

	static void InitMono() 
	{
		mono_set_dirs("..\\Toast\\vendor\\mono\\lib", "..\\Toast\\vendor\\mono\\etc");

		auto domain = mono_jit_init("Toast");

		char* name = (char*)"ToastRuntime";
		sMonoDomain = mono_domain_create_appdomain(name, nullptr);
	}

	static void ShutdownMono() 
	{
		mono_jit_cleanup(sMonoDomain);
	}

	MonoObject* EntityInstance::GetInstance()
	{
		TOAST_CORE_ASSERT(Handle, "Entity has not been instantiated!");
		return mono_gchandle_get_target(Handle);
	}

	static std::unordered_map<std::string, EntityScriptClass> sEntityClassMap;

	MonoAssembly* LoadAssemblyFromFile(const char* filepath)
	{
		if (filepath == NULL)
		{
			return NULL;
		}

		HANDLE file = CreateFileA(filepath, FILE_READ_ACCESS, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file == INVALID_HANDLE_VALUE)
		{
			return NULL;
		}

		DWORD file_size = GetFileSize(file, NULL);
		if (file_size == INVALID_FILE_SIZE)
		{
			CloseHandle(file);
			return NULL;
		}

		void* file_data = malloc(file_size);
		if (file_data == NULL)
		{
			CloseHandle(file);
			return NULL;
		}

		DWORD read = 0;
		ReadFile(file, file_data, file_size, &read, NULL);
		if (file_size != read)
		{
			free(file_data);
			CloseHandle(file);
			return NULL;
		}

		MonoImageOpenStatus status;
		MonoImage* image = mono_image_open_from_data_full(reinterpret_cast<char*>(file_data), file_size, 1, &status, 0);
		if (status != MONO_IMAGE_OK)
		{
			return NULL;
		}
		auto assemb = mono_assembly_load_from_full(image, filepath, &status, 0);
		free(file_data);
		CloseHandle(file);
		mono_image_close(image);
		return assemb;
	}

	static MonoMethod* GetMethod(MonoImage* image, const std::string& methodDesc)
	{
		std::string name = "ClientHelloWorld:OnCreate()";
		MonoMethodDesc* desc = mono_method_desc_new(name.c_str(), NULL);
		//MonoMethodDesc* desc = mono_method_desc_new(methodDesc.c_str(), NULL);
		if (!desc)
			TOAST_CORE_WARN("mono_method_desc_new failed");

		MonoMethod* method = mono_method_desc_search_in_image(desc, image);
		if (!method)
			TOAST_CORE_WARN("mono_method_desc_search_in_image failed");

		return method;
	}

	static MonoObject* CallMethod(MonoObject* object, MonoMethod* method, void** params = nullptr)
	{
		MonoObject* pException = nullptr;
		MonoObject* result = mono_runtime_invoke(method, object, params, &pException);
		return result;
	}

	static MonoAssembly* LoadAssembly(const std::string& path) 
	{
		MonoAssembly* assembly = LoadAssemblyFromFile(path.c_str());

		if (!assembly)
			TOAST_CORE_INFO("Could not load assembly: {0}", path);
		else
			TOAST_CORE_INFO("Successfully loaded assembly: {0}", path);

		return assembly;
	}

	static MonoImage* GetAssemblyImage(MonoAssembly* assembly)
	{
		MonoImage* image = mono_assembly_get_image(assembly);
		if (!image)
			TOAST_CORE_INFO("mono_assembly_get_image failed");

		return image;
	}

	static MonoClass* GetClass(MonoImage* image, const EntityScriptClass& scriptClass)
	{
		MonoClass* monoClass = mono_class_from_name(image, scriptClass.NamespaceName.c_str(), scriptClass.ClassName.c_str());
		if (!monoClass)
			TOAST_CORE_INFO("mono_class_from_name failed");

		return monoClass;
	}

	static uint32_t Instantiate(EntityScriptClass& scriptClass)
	{
		MonoObject* instance = mono_object_new(sMonoDomain, scriptClass.Class);
		if (!instance)
			std::cout << "mono_object_new failed" << std::endl;

		mono_runtime_object_init(instance);
		uint32_t handle = mono_gchandle_new(instance, false);
		return handle;
	}


	void ScriptEngine::Init(const std::string& assemblyPath)
	{
		sAssemblyPath = assemblyPath;

		InitMono();

		LoadToastRuntimeAssembly(sAssemblyPath);
	}

	void ScriptEngine::Shutdown()
	{
		sSceneContext = nullptr;
		sEntityInstanceMap.clear();
	}

	void ScriptEngine::LoadToastRuntimeAssembly(const std::string& path)
	{
		MonoDomain* domain = nullptr;
		bool cleanup = false;
		if (sMonoDomain)
		{
			domain = mono_domain_create_appdomain("Toast Runtime", nullptr);
			mono_domain_set(domain, false);

			cleanup = true;
		}

		sCoreAssembly = LoadAssembly("assets/scripts/Toast-ScriptCore.dll");
		sCoreAssemblyImage = GetAssemblyImage(sCoreAssembly);

		auto appAssembly = LoadAssembly(path);
		auto appAssemblyImage = GetAssemblyImage(appAssembly);

		//Register classes here

		if (cleanup)
		{
			mono_domain_unload(sMonoDomain);
			sMonoDomain = domain;
		}

		sAppAssembly = appAssembly;
		sAppAssemblyImage = appAssemblyImage;
	}

	void ScriptEngine::ReloadAssembly(const std::string& path)
	{
		LoadToastRuntimeAssembly(path);
		if (sEntityInstanceMap.size()) 
		{
			Scene* scene = ScriptEngine::GetCurrentSceneContext();
			TOAST_CORE_ASSERT(scene, "No active scene!");
			if (sEntityInstanceMap.find("Scene") != sEntityInstanceMap.end())
			{
				auto& entityMap = sEntityInstanceMap.at("Scene");
				for (auto& [entityID, entityInstanceData] : entityMap)
				{
					const auto& entityMap = scene->GetEntityMap();
					TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in scene!");
					InitScriptEntity(entityMap.at(entityID));
				}
			}

		}
	}

	void ScriptEngine::SetSceneContext(Scene* scene)
	{
		sSceneContext = scene;
	}

	Scene* ScriptEngine::GetCurrentSceneContext()
	{
		return sSceneContext;
	}

	void ScriptEngine::OnCreateEntity(Entity entity)
	{
		EntityInstance& entityInstance = GetEntityInstanceData("Scene", (uint32_t)entity).Instance;
		if (entityInstance.ScriptClass->OnCreateMethod)
			CallMethod(entityInstance.GetInstance(), entityInstance.ScriptClass->OnCreateMethod);
	}

	bool ScriptEngine::ModuleExists(const std::string& moduleName)
	{
		std::string NamespaceName, ClassName;
		if (moduleName.find('.') != std::string::npos)
		{
			NamespaceName = moduleName.substr(0, moduleName.find_last_of('.'));
			ClassName = moduleName.substr(moduleName.find_last_of('.') + 1);
		}
		else
			ClassName = moduleName;

		MonoClass* monoClass = mono_class_from_name(sAppAssemblyImage, NamespaceName.c_str(), ClassName.c_str());

		return monoClass != nullptr;
	}

	void ScriptEngine::InitScriptEntity(Entity entity)
	{
		Scene* scene = entity.mScene;
		auto& moduleName = entity.GetComponent<ScriptComponent>().ModuleName;
		if (moduleName.empty())
			return;

		if (!ModuleExists(moduleName))
		{
			TOAST_CORE_ERROR("Entity references non-existent script module '{0}'", moduleName);
			return;
		}

		EntityScriptClass& scriptClass = sEntityClassMap[moduleName];
		scriptClass.FullName = moduleName;
		if (moduleName.find('.') != std::string::npos)
		{
			scriptClass.NamespaceName = moduleName.substr(0, moduleName.find_last_of('.'));
			scriptClass.ClassName = moduleName.substr(moduleName.find_last_of('.') + 1);
		}
		else
			scriptClass.ClassName = moduleName;

		scriptClass.Class = GetClass(sAppAssemblyImage, scriptClass);
		scriptClass.InitClassMethods(sAppAssemblyImage);

		EntityInstanceData& entityInstanceData = sEntityInstanceMap["Scene"][(uint32_t)entity];
		EntityInstance& entityInstance = entityInstanceData.Instance;
		entityInstance.ScriptClass = &scriptClass;
	}

	void ScriptEngine::ShutdownScriptEntity(uint32_t entityID, const std::string& moduleName)
	{
		EntityInstanceData& entityInstanceData = GetEntityInstanceData("Scene", entityID);

		//Enter stuff to remove field map in the future
	}

	void ScriptEngine::InstantiateEntityClass(Entity entity)
	{
		Scene* scene = entity.mScene;
		uint32_t id = (uint32_t)entity;
		auto& moduleName = entity.GetComponent<ScriptComponent>().ModuleName;

		EntityInstanceData& entityInstanceData = GetEntityInstanceData("Scene", (uint32_t)entity);
		EntityInstance& entityInstance = entityInstanceData.Instance;
		TOAST_CORE_ASSERT(entityInstance.ScriptClass, "Script class in Entity Instance null");
		entityInstance.Handle = Instantiate(*entityInstance.ScriptClass);

		// Data for handling Fieldmap later on

		OnCreateEntity(entity);
	}

	Toast::EntityInstanceData& ScriptEngine::GetEntityInstanceData(const std::string& sceneName, uint32_t entityID)
	{
		TOAST_CORE_ASSERT(sEntityInstanceMap.find(sceneName) != sEntityInstanceMap.end(), "Invalid Scene Name!");
		auto& entityIDMap = sEntityInstanceMap.at(sceneName);
		TOAST_CORE_ASSERT(entityIDMap.find(entityID) != entityIDMap.end(), "Invalid entity ID!");
		return entityIDMap.at(entityID);
	}

}
