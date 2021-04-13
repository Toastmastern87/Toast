#include "tpch.h"
#include "ScriptEngine.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/attrdefs.h>

namespace Toast {

	static MonoDomain* sMonoDomain = nullptr;
	static std::string sAssemblyPath;

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
		MonoMethodDesc* desc = mono_method_desc_new(methodDesc.c_str(), NULL);
		if (!desc)
			TOAST_CORE_WARN("mono_method_desc_new failed");

		MonoMethod* method = mono_method_desc_search_in_image(desc, image);
		if (!method)
			TOAST_CORE_WARN("mono_method_desc_search_in_image failed");

		return method;
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

	void ScriptEngine::Init(const std::string& assemblyPath)
	{
		mono_set_dirs("..\\Toast\\vendor\\mono\\lib", "..\\Toast\\vendor\\mono\\etc");

		sAssemblyPath = assemblyPath;

		auto domain = mono_jit_init("Toast");

		char* name = (char*)"ToastRuntime";
		sMonoDomain = mono_domain_create_appdomain(name, nullptr);

		LoadToastRuntimeAssembly(sAssemblyPath);
	}

	void ScriptEngine::Shutdown()
	{
		mono_jit_cleanup(sMonoDomain);
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

	bool ScriptEngine::ModuleExists(const std::string& moduleName)
	{
		std::string NamespaceName, ClassName;
		if (moduleName.find('.') != std::string::npos)
		{
			NamespaceName = moduleName.substr(0, moduleName.find_last_of('.'));
			ClassName = moduleName.substr(moduleName.find_last_of('.') + 1);
		}
		else
		{
			ClassName = moduleName;
		}

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
		{
			scriptClass.ClassName = moduleName;
		}

		scriptClass.Class = GetClass(sAppAssemblyImage, scriptClass);
		scriptClass.InitClassMethods(sAppAssemblyImage);

		EntityInstanceData& entityInstanceData = sEntityInstanceMap[&scene][entity];
		EntityInstance& entityInstance = entityInstanceData.Instance;
		entityInstance.ScriptClass = &scriptClass;



	}

}
