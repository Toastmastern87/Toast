#include "tpch.h"
#include "ScriptEngine.h"

#include "Toast/Scripting/ScriptGlue.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/tabledefs.h>

#include "FileWatch.h"

#include "Toast/Core/Application.h"
#include "Toast/Core/Buffer.h"
#include "Toast/Core/FileSystem.h"

namespace Toast {

	static std::unordered_map<std::string, ScriptFieldType> sScriptFieldTypeMap =
	{
		{ "System.Single", ScriptFieldType::Float },
		{ "System.Double", ScriptFieldType::Double },
		{ "System.Boolean", ScriptFieldType::Bool },
		{ "System.Char", ScriptFieldType::Char },
		{ "System.Int16", ScriptFieldType::Short },
		{ "System.Int32", ScriptFieldType::Int },
		{ "System.Int64", ScriptFieldType::Long },
		{ "System.Byte", ScriptFieldType::Byte },
		{ "System.UInt16", ScriptFieldType::UShort },
		{ "System.UInt32", ScriptFieldType::UInt },
		{ "System.UInt64", ScriptFieldType::ULong },

		{ "Toast.Vector2", ScriptFieldType::Vector2 },
		{ "Toast.Vector3", ScriptFieldType::Vector3 },
		{ "Toast.Vector4", ScriptFieldType::Vector4 },

		{ "Toast.Entity", ScriptFieldType::Entity },
	};

	namespace Utils 
	{

		static MonoAssembly* LoadMonoAssembly(const std::filesystem::path& assemblyPath)
		{
			Buffer fileData = FileSystem::ReadFileBinary(assemblyPath);

			// NOTE: We can't use this image for anything other than loading the assembly because this image doesn't have a reference to the assembly
			MonoImageOpenStatus status;
			MonoImage* image = mono_image_open_from_data_full(&fileData.Read<char>(), fileData.Size, 1, &status, 0);

			if (status != MONO_IMAGE_OK)
			{
				const char* errorMessage = mono_image_strerror(status);
				return nullptr;
			}

			std::string pathString = assemblyPath.string();
			MonoAssembly* assembly = mono_assembly_load_from_full(image, pathString.c_str(), &status, 0);
			mono_image_close(image);

			fileData.Release();

			return assembly;
		}

		void PrintAssemblyTypes(MonoAssembly* assembly)
		{
			MonoImage* image = mono_assembly_get_image(assembly);
			const MonoTableInfo* typeDefinitionsTable = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
			int32_t numTypes = mono_table_info_get_rows(typeDefinitionsTable);

			for (int32_t i = 0; i < numTypes; i++)
			{
				uint32_t cols[MONO_TYPEDEF_SIZE];
				mono_metadata_decode_row(typeDefinitionsTable, i, cols, MONO_TYPEDEF_SIZE);

				const char* nameSpace = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
				const char* name = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);

				TOAST_CORE_INFO("%s.%s", nameSpace, name);
			}
		}

		ScriptFieldType MonoTypeToScriptFieldType(MonoType* monoType)
		{
			std::string typeName = mono_type_get_name(monoType);

			auto it = sScriptFieldTypeMap.find(typeName);
			if (it == sScriptFieldTypeMap.end())
				return ScriptFieldType::None;

			return it->second;
		}

	}

	struct ScriptEngineData
	{
		MonoDomain* RootDomain = nullptr;
		MonoDomain* AppDomain = nullptr;

		MonoAssembly* CoreAssembly = nullptr;
		MonoImage* CoreAssemblyImage = nullptr;

		MonoAssembly* AppAssembly = nullptr;
		MonoImage* AppAssemblyImage = nullptr;

		std::filesystem::path CoreAssemblyFilepath;
		std::filesystem::path AppAssemblyFilepath;

		ScriptClass EntityClass;

		std::unordered_map<std::string, Ref<ScriptClass>> EntityClasses;
		std::unordered_map<UUID, Ref<ScriptInstance>> EntityInstances;
		std::unordered_map<UUID, ScriptFieldMap> EntityScriptFields;

		Scope<filewatch::FileWatch<std::string>> AppAssemblyFileWatcher;
		bool AssemblyReloadPending = false;

		// Runtime
		Scene* SceneContext = nullptr;
	};

	static ScriptEngineData* sData = nullptr;

	static void OnAppAssemblyFileSystemEvent(const std::string& path, const filewatch::Event change_type)
	{
		if (!sData->AssemblyReloadPending && change_type == filewatch::Event::modified)
		{
			sData->AssemblyReloadPending = true;

			Application::Get().SubmitToMainThread([]()
				{
					sData->AppAssemblyFileWatcher.reset();
					ScriptEngine::ReloadAssembly();
				});
		}
	}

	void ScriptEngine::Init()
	{
		sData = new ScriptEngineData();

		InitMono();
		ScriptGlue::RegisterFunctions();

		bool status = LoadAssembly("assets/scripts/Toast-ScriptCore.dll");
		if(!status)
		{
			TOAST_CORE_ERROR("[ScriptEngine] Could not load Toast-ScriptCore aseembly.");
			return;
		}

		status = LoadAppAssembly("SandboxProject/Assets/Scripts/Binaries/Sandbox.dll");
		if (!status)
		{
			TOAST_CORE_ERROR("[ScriptEngine] Could not load app aseembly.");
			return;
		}

		LoadAssemblyClasses();

		ScriptGlue::RegisterComponents();

		// Retrieve and instantiate entity class (with constructor)
		sData->EntityClass = ScriptClass("Toast", "Entity", true);

	}

	void ScriptEngine::Shutdown()
	{
		ShutdownMono();

		delete sData;
	}

	void ScriptEngine::InitMono()
	{
		mono_set_assemblies_path("mono/lib");

		// Create the root domain
		MonoDomain* rootDomain = mono_jit_init("ToastJITRuntime");
		TOAST_CORE_ASSERT(rootDomain, "Root domain is a nullptr");

		// Store the root domain pointer
		sData->RootDomain = rootDomain;
	}

	void ScriptEngine::ShutdownMono()
	{
		mono_domain_set(mono_get_root_domain(), false);

		mono_domain_unload(sData->AppDomain);
		sData->AppDomain = nullptr;

		mono_jit_cleanup(sData->RootDomain);
		sData->RootDomain = nullptr;
	}

	bool ScriptEngine::LoadAssembly(const std::filesystem::path& filepath)
	{
		sData->AppDomain = mono_domain_create_appdomain("ToastScriptRuntime", nullptr);
		mono_domain_set(sData->AppDomain, true);

		sData->CoreAssemblyFilepath = filepath;
		sData->CoreAssembly = Utils::LoadMonoAssembly(filepath);

		if (sData->CoreAssembly == nullptr)
			return false;

		sData->CoreAssemblyImage = mono_assembly_get_image(sData->CoreAssembly);

		return true;
	}

	bool ScriptEngine::LoadAppAssembly(const std::filesystem::path& filepath)
	{
		sData->AppAssemblyFilepath = filepath;
		sData->AppAssembly = Utils::LoadMonoAssembly(filepath);

		if (sData->AppAssembly == nullptr)
			return false;

		sData->AppAssemblyImage = mono_assembly_get_image(sData->AppAssembly);

		sData->AppAssemblyFileWatcher = CreateScope<filewatch::FileWatch<std::string>>(filepath.string(), OnAppAssemblyFileSystemEvent);
		sData->AssemblyReloadPending = false;

		return true;
	}

	void ScriptEngine::ReloadAssembly()
	{
		mono_domain_set(mono_get_root_domain(), false);

		mono_domain_unload(sData->AppDomain);

		LoadAssembly(sData->CoreAssemblyFilepath);
		LoadAppAssembly(sData->AppAssemblyFilepath);

		LoadAssemblyClasses();

		ScriptGlue::RegisterComponents();

		sData->EntityClass = ScriptClass("Toast", "Entity", true);
	}

	void ScriptEngine::OnRuntimeStart(Scene* scene)
	{
		sData->SceneContext = scene;
	}

	void ScriptEngine::OnRuntimeStop()
	{
		sData->SceneContext = nullptr;

		sData->EntityInstances.clear();
	}

	bool ScriptEngine::EntityClassExists(const std::string& fullClassName)
	{
		return sData->EntityClasses.find(fullClassName) != sData->EntityClasses.end();
	}

	void ScriptEngine::OnCreateEntity(Entity entity)
	{
		const auto& sc = entity.GetComponent<ScriptComponent>();
		if (ScriptEngine::EntityClassExists(sc.ClassName))
		{
			UUID entityID = entity.GetUUID();

			Ref<ScriptInstance> instance = CreateRef<ScriptInstance>(sData->EntityClasses[sc.ClassName], entity);
			sData->EntityInstances[entityID] = instance;

			// Copy field values
			if (sData->EntityScriptFields.find(entityID) != sData->EntityScriptFields.end())
			{
				const ScriptFieldMap& fieldMap = sData->EntityScriptFields.at(entityID);
				for (const auto& [name, fieldInstance] : fieldMap)
					instance->SetFieldValueInternal(name, fieldInstance.mBuffer);
			}

			instance->InvokeOnCreate();
		}
	}

	void ScriptEngine::OnUpdateEntity(Entity entity, Timestep ts)
	{
		UUID entityUUID = entity.GetUUID();
		if (sData->EntityInstances.find(entity.GetUUID()) != sData->EntityInstances.end())
		{
			Ref<ScriptInstance> instance = sData->EntityInstances[entityUUID];
			instance->InvokeOnUpdate((float)ts);
		}
		else
		{
			TOAST_CORE_ERROR("Could not find ScriptInstance for entity instance %d", entityUUID);
		}
	}

	void ScriptEngine::OnEventEntity(Entity entity)
	{
		UUID entityUUID = entity.GetUUID();
		TOAST_CORE_ASSERT(sData->EntityInstances.find(entity.GetUUID()) != sData->EntityInstances.end(), "Entity Instance does not exist!");

		Ref<ScriptInstance> instance = sData->EntityInstances[entityUUID];

		const auto& sc = entity.GetComponent<ScriptComponent>();
		instance->InvokeOnEvent();
	}

	Scene* ScriptEngine::GetSceneContext()
	{
		return sData->SceneContext;
	}

	Ref<ScriptInstance> ScriptEngine::GetEntityScriptInstance(UUID entityID)
	{
		auto it = sData->EntityInstances.find(entityID);
		if (it == sData->EntityInstances.end())
			return nullptr;

		return it->second;
	}

	Ref<ScriptClass> ScriptEngine::GetEntityClass(const std::string& name)
	{
		if (sData->EntityClasses.find(name) == sData->EntityClasses.end())
			return nullptr;

		return sData->EntityClasses.at(name);
	}

	std::unordered_map<std::string, Ref<ScriptClass>> ScriptEngine::GetEntityClasses()
	{
		return sData->EntityClasses;
	}

	ScriptFieldMap& ScriptEngine::GetScriptFieldMap(Entity entity)
	{
		TOAST_CORE_ASSERT(entity, "");

		UUID entityID = entity.GetUUID();

		return sData->EntityScriptFields[entityID];
	}

	void ScriptEngine::LoadAssemblyClasses()
	{
		sData->EntityClasses.clear();
		const MonoTableInfo* typeDefinitionsTable = mono_image_get_table_info(sData->AppAssemblyImage, MONO_TABLE_TYPEDEF);
		int32_t numTypes = mono_table_info_get_rows(typeDefinitionsTable);
		MonoClass* entityClass = mono_class_from_name(sData->CoreAssemblyImage, "Toast", "Entity");

		for (int32_t i = 0; i < numTypes; i++)
		{
			uint32_t cols[MONO_TYPEDEF_SIZE];
			mono_metadata_decode_row(typeDefinitionsTable, i, cols, MONO_TYPEDEF_SIZE);

			const char* nameSpace = mono_metadata_string_heap(sData->AppAssemblyImage, cols[MONO_TYPEDEF_NAMESPACE]);
			const char* className = mono_metadata_string_heap(sData->AppAssemblyImage, cols[MONO_TYPEDEF_NAME]);
			std::string fullName;
			if (strlen(nameSpace) != 0)
			{
				fullName.append(nameSpace);
				fullName.append(".");
				fullName.append(className);
			}
			else
				fullName = className;

			MonoClass* monoClass = mono_class_from_name(sData->AppAssemblyImage, nameSpace, className);

			if (monoClass == entityClass)
				continue;

			bool isEntity = mono_class_is_subclass_of(monoClass, entityClass, false);
			if (!isEntity)
				continue;

			Ref<ScriptClass> scriptClass = CreateRef<ScriptClass>(nameSpace, className);
			sData->EntityClasses[fullName] = scriptClass;

			int fieldCount = mono_class_num_fields(monoClass);

			TOAST_CORE_WARN("%s has %d fields: ", className, fieldCount);
			void* iterator = nullptr;
			while (MonoClassField* field = mono_class_get_fields(monoClass, &iterator))
			{
				const char* fieldName = mono_field_get_name(field);
				uint32_t flags = mono_field_get_flags(field);
				if (flags & FIELD_ATTRIBUTE_PUBLIC)
				{
					MonoType* type = mono_field_get_type(field);
					ScriptFieldType fieldType = Utils::MonoTypeToScriptFieldType(type);
					TOAST_CORE_WARN("   %s (%s)", fieldName, Utils::ScriptFieldTypeToString(fieldType));

					scriptClass->mFields[fieldName] = { fieldName, fieldType, field };
				}
			}

			//mono_field_get_value()
		}
	}

	MonoImage* ScriptEngine::GetCoreAssemblyImage()
	{
		return sData->CoreAssemblyImage;
	}

	MonoObject* ScriptEngine::GetManagedInstance(UUID uuid)
	{
		TOAST_CORE_ASSERT(sData->EntityInstances.find(uuid) != sData->EntityInstances.end(uuid), "");
		return sData->EntityInstances.at(uuid)->GetManagedObject();
	}

	MonoObject* ScriptEngine::InstantiateClass(MonoClass* monoClass)
	{
		MonoObject* instance = mono_object_new(sData->AppDomain, monoClass);
		mono_runtime_object_init(instance);
		return instance;
	}

	ScriptClass::ScriptClass(const std::string& classNamespace, const std::string& className, bool isCore)
		: mClassNamespace(classNamespace), mClassName(className)
	{
		mMonoClass = mono_class_from_name(isCore ? sData->CoreAssemblyImage : sData->AppAssemblyImage, classNamespace.c_str(), className.c_str());
	}

	MonoObject* ScriptClass::Instantiate()
	{
		return ScriptEngine::InstantiateClass(mMonoClass);
	}

	MonoMethod* ScriptClass::GetMethod(const std::string& name, int parameterCount)
	{
		return mono_class_get_method_from_name(mMonoClass, name.c_str(), parameterCount);
	}

	MonoObject* ScriptClass::InvokeMethod(MonoObject* instance, MonoMethod* method, void** params)
	{
		return mono_runtime_invoke(method, instance, params, nullptr);
	}

	ScriptInstance::ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity)
		: mScriptClass(scriptClass)
	{
		mInstance = scriptClass->Instantiate();
		mConstructor = sData->EntityClass.GetMethod(".ctor", 1);
		mOnCreateMethod = scriptClass->GetMethod("OnCreate", 0);
		mOnUpdateMethod = scriptClass->GetMethod("OnUpdate", 1);
		mOnEventMethod = scriptClass->GetMethod("OnEvent", 0);

		// Call Entity Constructor
		{
			UUID entityID = entity.GetUUID();
			void* param = &entityID;
			mScriptClass->InvokeMethod(mInstance, mConstructor, &param);
		}
	}

	void ScriptInstance::InvokeOnCreate()
	{
		if (mOnCreateMethod)
		{
			sData->EntityClass.InvokeMethod(mInstance, mOnCreateMethod);
		}
	}

	void ScriptInstance::InvokeOnUpdate(float ts)
	{
		if (mOnUpdateMethod)
		{
			void* param = &ts;
			mScriptClass->InvokeMethod(mInstance, mOnUpdateMethod, &param);
		}
	}

	void ScriptInstance::InvokeOnEvent()
	{
		mScriptClass->InvokeMethod(mInstance, mOnEventMethod);
	}

	bool ScriptInstance::GetFieldValueInternal(const std::string& name, void* buffer)
	{
		const auto& fields = mScriptClass->GetFields();
		auto it = fields.find(name);

		if (it == fields.end())
			return nullptr;

		const ScriptField& field = it->second;
		void* result;
		mono_field_get_value(mInstance, field.ClassField, buffer);

		return true;
	}

	bool ScriptInstance::SetFieldValueInternal(const std::string& name, const void* value)
	{
		const auto& fields = mScriptClass->GetFields();
		auto it = fields.find(name);

		if (it == fields.end())
			return nullptr;

		const ScriptField& field = it->second;
		void* result;
		mono_field_set_value(mInstance, field.ClassField, (void*)value);

		return true;
	}

}