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

		std::filesystem::path ScriptSourceRoot;
		std::unordered_map<std::string, std::filesystem::path> ClassSourcePaths;

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

		// TODO: Source root for the class -> .cs lookup. Derived from the assembly path
		// for now; switch to the asset system once .cs becomes a real asset type.
		sData->ScriptSourceRoot = filepath.parent_path().parent_path() / "Source";

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

	void ScriptEngine::OnCreateEntityWithClass(Entity entity, const std::string& className)
	{
		if (ScriptEngine::EntityClassExists(className))
		{
			UUID entityID = entity.GetUUID();
			Ref<ScriptInstance> instance = CreateRef<ScriptInstance>(sData->EntityClasses[className], entity);
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

	void ScriptEngine::OnCreateEntity(Entity entity)
	{
		const auto& sc = entity.GetComponent<ScriptComponent>();
		OnCreateEntityWithClass(entity, sc.ClassName);
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
			TOAST_CORE_ERROR("Could not find ScriptInstance for entity instance %d", entityUUID);
	}

	void ScriptEngine::OnEventEntity(Entity entity)
	{
		UUID entityUUID = entity.GetUUID();
		TOAST_CORE_ASSERT(sData->EntityInstances.find(entity.GetUUID()) != sData->EntityInstances.end(), "Entity Instance does not exist!");

		Ref<ScriptInstance> instance = sData->EntityInstances[entityUUID];
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

	const std::unordered_map<std::string, Ref<ScriptClass>>& ScriptEngine::GetEntityClasses()
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
		sData->ClassSourcePaths.clear();

		if (std::filesystem::exists(sData->ScriptSourceRoot))
		{
			for (auto& entry : std::filesystem::recursive_directory_iterator(sData->ScriptSourceRoot))
			{
				if (entry.path().extension() != ".cs")
					continue;

				sData->ClassSourcePaths[entry.path().stem().string()] = entry.path();
			}
		}

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
			if (!monoClass)
			{
				TOAST_CORE_CRITICAL("Failed to resolve type: %s.%s", nameSpace, className);
				continue;
			}

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

	std::filesystem::path ScriptEngine::GetEntityClassSourcePath(const std::string& fullClassName)
	{
		if (auto it = sData->ClassSourcePaths.find(fullClassName); it != sData->ClassSourcePaths.end())
			return it->second;

		size_t dot = fullClassName.find_last_of('.');
		std::string shortName = (dot == std::string::npos) ? fullClassName : fullClassName.substr(dot + 1);

		if (auto it = sData->ClassSourcePaths.find(shortName); it != sData->ClassSourcePaths.end())
			return it->second;

		return {};
	}

	const std::filesystem::path& ScriptEngine::GetScriptSourceRoot()
	{
		return sData->ScriptSourceRoot;
	}

	uint32_t ScriptEngine::InstantiateClass(MonoClass* monoClass)
	{
		MonoObject* instance = mono_object_new(sData->AppDomain, monoClass);
		int32_t instanceHandle = mono_gchandle_new(instance, false);
		mono_runtime_object_init(instance);
		return instanceHandle;
	}

	bool ScriptEngine::IsValidIdentifier(const std::string& name)
	{
		if (name.empty())
			return false;

		// C# identifiers must start with a letter or underscore...
		if (!std::isalpha((unsigned char)name[0]) && name[0] != '_')
			return false;

		// ...and contain only letters, digits, or underscores.
		for (char c : name)
			if (!std::isalnum((unsigned char)c) && c != '_')
				return false;

		return true;
	}

	static const char* sEntityScriptTemplate = R"(using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;

using Toast;

namespace {NAMESPACE}
{
    public class {CLASSNAME} : Entity
    {
        void OnCreate()
        {
        }

        void OnEvent()
        {
        }

        void OnUpdate(float ts)
        {
        }
    }
}
)";

	bool ScriptEngine::WriteScriptTemplate(const std::filesystem::path& target, const std::string& nameSpace, const std::string& className)
	{
		std::string contents = sEntityScriptTemplate;

		auto replaceAll = [](std::string& str, const std::string& from, const std::string& to)
			{
				size_t pos = 0;
				while ((pos = str.find(from, pos)) != std::string::npos)
				{
					str.replace(pos, from.length(), to);
					pos += to.length();   // skip past the inserted text
				}
			};

		replaceAll(contents, "{NAMESPACE}", nameSpace);
		replaceAll(contents, "{CLASSNAME}", className);

		// Target may sit in a sub folder that does not exist yet.
		std::filesystem::create_directories(target.parent_path());

		std::ofstream out(target, std::ios::out | std::ios::binary);
		if (!out)
		{
			TOAST_CORE_ERROR("Failed to create script: %s", target.string().c_str());
			return false;
		}

		out << contents;
		return true;
	}

	bool ScriptEngine::CompileScripts(const std::filesystem::path& sourceDir, const std::filesystem::path& outputDll)
	{
		if (!std::filesystem::exists(sourceDir))
		{
			TOAST_CORE_ERROR("[Coompile] Script source folder does not exists: %s", sourceDir.string().c_str());
			return false;
		}

		// Gather all the source files
		std::vector<std::filesystem::path> sources;
		for (auto& entry : std::filesystem::recursive_directory_iterator(sourceDir)) 
		{
			if (entry.path().extension() == ".cs")
				sources.push_back(entry.path());
		}

		if (sources.empty())
		{
			TOAST_CORE_WARN("[Compile] No .cs files found in %s", sourceDir.string().c_str());
			return false;
		}

		// Build the compile command
		std::filesystem::create_directories(outputDll.parent_path());

		std::string cmd;
		cmd += "\"\"mono\\bin\\mono.exe\"";                        // launcher
		cmd += " \"mono\\lib\\mono\\4.5\\mcs.exe\"";               // compiler (managed)
		cmd += " -target:library";
		cmd += " -out:\"" + outputDll.string() + "\"";
		cmd += " -r:\"assets\\scripts\\Toast-ScriptCore.dll\"";    // the 'using Toast;' reference

		for (const auto& src : sources)
			cmd += " \"" + src.string() + "\"";

		cmd += " 2>&1\"";   // fold stderr into stdout — mcs writes diagnostics to stderr

		// Run command and capture the output
		TOAST_CORE_INFO("[Compile] Compiling %d script(s) -> %s", (int)sources.size(), outputDll.string().c_str());

		FILE* pipe = _popen(cmd.c_str(), "r");
		if (!pipe) 
		{
			TOAST_CORE_ERROR("[Compile] Failed to launch compiler process");
			return false;
		}

		char buffer[512];
		std::string output;
		while (fgets(buffer, sizeof(buffer), pipe))
			output += buffer;

		int exitCode = _pclose(pipe);

		// Report
		if (!output.empty())
		{
			std::istringstream stream(output);
			std::string line;
			while (std::getline(stream, line))
			{
				if (line.empty())
					continue;

				if (line.find(": error") != std::string::npos)
					TOAST_CORE_ERROR("[Compile] %s", line.c_str());
				else if (line.find(": warning") != std::string::npos)
					TOAST_CORE_WARN("[Compile] %s", line.c_str());
				else
					TOAST_CORE_INFO("[Compile] %s", line.c_str());
			}
		}

		if (exitCode != 0)
		{
			TOAST_CORE_ERROR("[Compile] Failed (exit code %d)", exitCode);
			return false;
		}

		TOAST_CORE_INFO("[Compile] Succeeded: %s", outputDll.string().c_str());
		return true;
	}

	ScriptClass::ScriptClass(const std::string& classNamespace, const std::string& className, bool isCore)
		: mClassNamespace(classNamespace), mClassName(className)
	{
		mMonoClass = mono_class_from_name(isCore ? sData->CoreAssemblyImage : sData->AppAssemblyImage, classNamespace.c_str(), className.c_str());
	}

	uint32_t ScriptClass::Instantiate()
	{
		return ScriptEngine::InstantiateClass(mMonoClass);
	}

	MonoMethod* ScriptClass::GetMethod(const std::string& name, int parameterCount)
	{
		return mono_class_get_method_from_name(mMonoClass, name.c_str(), parameterCount);
	}

	MonoObject* ScriptClass::InvokeMethod(uint32_t instance, MonoMethod* method, void** params)
	{
		MonoObject* monoObject = mono_gchandle_get_target(instance);
		MonoObject* exception = nullptr;
		return mono_runtime_invoke(method, monoObject, params, &exception);

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
			mScriptClass->InvokeMethod(mInstance, mOnCreateMethod);
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
		if(mOnEventMethod)
			mScriptClass->InvokeMethod(mInstance, mOnEventMethod);
	}

	MonoObject* ScriptInstance::GetManagedObject()
	{
		MonoObject* managedObject = mono_gchandle_get_target(mInstance);

		return managedObject;
	}

	bool ScriptInstance::GetFieldValueInternal(const std::string& name, void* buffer)
	{
		const auto& fields = mScriptClass->GetFields();
		auto it = fields.find(name);

		if (it == fields.end())
			return nullptr;

		const ScriptField& field = it->second;
		void* result;
		MonoObject* monoObject = mono_gchandle_get_target(mInstance);
		mono_field_get_value(monoObject, field.ClassField, buffer);

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
		MonoObject* monoObject = mono_gchandle_get_target(mInstance);
		mono_field_set_value(monoObject, field.ClassField, (void*)value);

		return true;
	}

}