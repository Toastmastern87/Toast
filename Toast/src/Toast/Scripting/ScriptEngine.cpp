#include "tpch.h"
#include "ScriptEngine.h"

//#include "Toast/Script/ScriptEngineRegistry.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
//#include <mono/metadata/debug-helpers.h>
//#include <mono/metadata/attrdefs.h>

namespace Toast {

	struct ScriptEngineData
	{
		MonoDomain* RootDomain = nullptr;
		MonoDomain* AppDomain = nullptr;

		MonoAssembly* CoreAssembly = nullptr;
	};

	static ScriptEngineData* sData = nullptr;
	//static MonoDomain* sMonoDomain = nullptr;
	//static std::string sAssemblyPath;
	//static Ref<Scene> sSceneContext;

	//// Assembly images
	//MonoImage* sAppAssemblyImage = nullptr;
	//MonoImage* sCoreAssemblyImage = nullptr;

	//static EntityInstanceMap sEntityInstanceMap;

	//static MonoAssembly* sAppAssembly = nullptr; 
	//static MonoAssembly* sCoreAssembly = nullptr;

	//static MonoMethod* GetMethod(MonoImage* image, const std::string& methodDesc);

	//struct EntityScriptClass 
	//{
	//	std::string FullName;
	//	std::string ClassName;
	//	std::string NamespaceName;

	//	MonoClass* Class = nullptr;
	//	MonoMethod* OnCreateMethod = nullptr;
	//	MonoMethod* OnUpdateMethod = nullptr;
	//	MonoMethod* OnClickMethod = nullptr;

	//	void InitClassMethods(MonoImage* image)
	//	{
	//		OnCreateMethod = GetMethod(image, FullName + ":OnCreate()");
	//		OnUpdateMethod = GetMethod(image, FullName + ":OnUpdate(single)");
	//		OnClickMethod = GetMethod(image, FullName + ":OnClick()");
	//	}
	//};

	//static void InitMono() 
	//{
	//	mono_set_dirs("..\\Toast\\vendor\\mono\\lib", "..\\Toast\\vendor\\mono\\etc");

	//	auto domain = mono_jit_init("Toast");

	//	char* name = (char*)"ToastRuntime";
	//	sMonoDomain = mono_domain_create_appdomain(name, nullptr);
	//}

	//static void ShutdownMono() 
	//{
	//	mono_jit_cleanup(sMonoDomain);
	//}

	//MonoObject* EntityInstance::GetInstance()
	//{
	//	TOAST_CORE_ASSERT(Handle, "Entity has not been instantiated!");
	//	return mono_gchandle_get_target(Handle);
	//}

	//static std::unordered_map<std::string, EntityScriptClass> sEntityClassMap;

	//MonoAssembly* LoadAssemblyFromFile(const char* filepath)
	//{
	//	if (filepath == NULL)
	//	{
	//		return NULL;
	//	}

	//	HANDLE file = CreateFileA(filepath, FILE_READ_ACCESS, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	//	if (file == INVALID_HANDLE_VALUE)
	//	{
	//		return NULL;
	//	}

	//	DWORD file_size = GetFileSize(file, NULL);
	//	if (file_size == INVALID_FILE_SIZE)
	//	{
	//		CloseHandle(file);
	//		return NULL;
	//	}

	//	void* file_data = malloc(file_size);
	//	if (file_data == NULL)
	//	{
	//		CloseHandle(file);
	//		return NULL;
	//	}

	//	DWORD read = 0;
	//	ReadFile(file, file_data, file_size, &read, NULL);
	//	if (file_size != read)
	//	{
	//		free(file_data);
	//		CloseHandle(file);
	//		return NULL;
	//	}

	//	MonoImageOpenStatus status;
	//	MonoImage* image = mono_image_open_from_data_full(reinterpret_cast<char*>(file_data), file_size, 1, &status, 0);
	//	if (status != MONO_IMAGE_OK)
	//	{
	//		return NULL;
	//	}
	//	auto assemb = mono_assembly_load_from_full(image, filepath, &status, 0);
	//	free(file_data);
	//	CloseHandle(file);
	//	mono_image_close(image);
	//	return assemb;
	//}

	//static MonoMethod* GetMethod(MonoImage* image, const std::string& methodDesc)
	//{
	//	MonoMethodDesc* desc = mono_method_desc_new(methodDesc.c_str(), NULL);
	//	if (!desc)
	//		TOAST_CORE_WARN("mono_method_desc_new failed");

	//	MonoMethod* method = mono_method_desc_search_in_image(desc, image);
	//	if (!method)
	//		TOAST_CORE_WARN("mono_method_desc_search_in_image failed");

	//	return method;
	//}

	//static MonoObject* CallMethod(MonoObject* object, MonoMethod* method, void** params = nullptr)
	//{
	//	MonoObject* pException = nullptr;
	//	MonoObject* result = mono_runtime_invoke(method, object, params, &pException);
	//	return result;
	//}

	//static MonoAssembly* LoadAssembly(const std::string& path) 
	//{
	//	MonoAssembly* assembly = LoadAssemblyFromFile(path.c_str());

	//	if (!assembly)
	//		TOAST_CORE_WARN("Could not load assembly: %s", path.c_str());
	//	else
	//		TOAST_CORE_INFO("Successfully loaded assembly: %s", path.c_str());

	//	return assembly;
	//}

	//static MonoImage* GetAssemblyImage(MonoAssembly* assembly)
	//{
	//	MonoImage* image = mono_assembly_get_image(assembly);
	//	if (!image)
	//		TOAST_CORE_WARN("mono_assembly_get_image failed");

	//	return image;
	//}

	//static MonoClass* GetClass(MonoImage* image, const EntityScriptClass& scriptClass)
	//{
	//	MonoClass* monoClass = mono_class_from_name(image, scriptClass.NamespaceName.c_str(), scriptClass.ClassName.c_str());
	//	if (!monoClass)
	//		TOAST_CORE_WARN("mono_class_from_name failed");

	//	return monoClass;
	//}

	//static uint32_t Instantiate(EntityScriptClass& scriptClass)
	//{
	//	MonoObject* instance = mono_object_new(sMonoDomain, scriptClass.Class);
	//	if (!instance)
	//		TOAST_CORE_WARN("mono_object_new failed");

	//	mono_runtime_object_init(instance);
	//	uint32_t handle = mono_gchandle_new(instance, false);
	//	return handle;
	//}

	//void ScriptEngine::Init(const std::string& assemblyPath)
	//{
	//	sAssemblyPath = assemblyPath;

	//	InitMono();

	//	LoadToastRuntimeAssembly(sAssemblyPath);
	//}

	//void ScriptEngine::Shutdown()
	//{
	//	sSceneContext = nullptr;
	//	sEntityInstanceMap.clear();
	//}

	//void ScriptEngine::LoadToastRuntimeAssembly(const std::string& path)
	//{
	//	MonoDomain* domain = nullptr;
	//	bool cleanup = false;
	//	if (sMonoDomain)
	//	{
	//		domain = mono_domain_create_appdomain("Toast Runtime", nullptr);
	//		mono_domain_set(domain, false);

	//		cleanup = true;
	//	}

	//	sCoreAssembly = LoadAssembly("assets/scripts/Toast-ScriptCore.dll");
	//	sCoreAssemblyImage = GetAssemblyImage(sCoreAssembly);

	//	auto appAssembly = LoadAssembly(path);
	//	auto appAssemblyImage = GetAssemblyImage(appAssembly);

	//	ScriptEngineRegistry::RegisterAll();

	//	if (cleanup)
	//	{
	//		mono_domain_unload(sMonoDomain);
	//		sMonoDomain = domain;
	//	}

	//	sAppAssembly = appAssembly;
	//	sAppAssemblyImage = appAssemblyImage;
	//}

	//void ScriptEngine::ReloadAssembly(const std::string& path)
	//{
	//	LoadToastRuntimeAssembly(path);
	//	if (sEntityInstanceMap.size()) 
	//	{
	//		Ref<Scene> scene = ScriptEngine::GetCurrentSceneContext();
	//		TOAST_CORE_ASSERT(scene, "No active scene!");
	//		if (sEntityInstanceMap.find(scene->GetUUID()) != sEntityInstanceMap.end())
	//		{
	//			auto& entityMap = sEntityInstanceMap.at(scene->GetUUID());
	//			for (auto& [entityID, entityInstanceData] : entityMap)
	//			{
	//				const auto& entityMap = scene->GetEntityMap();
	//				TOAST_CORE_ASSERT(entityMap.find(entityID) != entityMap.end(), "Invalid entity ID or entity doesn't exist in scene!");
	//				InitScriptEntity(entityMap.at(entityID));
	//			}
	//		}

	//	}
	//}

	//void ScriptEngine::SetSceneContext(const Ref<Scene>& scene)
	//{
	//	sSceneContext = scene;
	//}

	//const Ref<Scene>& ScriptEngine::GetCurrentSceneContext()
	//{
	//	return sSceneContext;
	//}

	//void ScriptEngine::CopyEntityScriptData(UUID dst, UUID src)
	//{
	//	TOAST_CORE_ASSERT(sEntityInstanceMap.find(dst) != sEntityInstanceMap.end(), "");
	//	TOAST_CORE_ASSERT(sEntityInstanceMap.find(src) != sEntityInstanceMap.end(), "");

	//	auto& dstEntityMap = sEntityInstanceMap.at(dst);
	//	auto& srcEntityMap = sEntityInstanceMap.at(src);

	//	for (auto& [entityID, entityInstanceData] : srcEntityMap)
	//	{
	//		for (auto& [moduleName, srcPropertyMap] : srcEntityMap[entityID].ModulePropertyMap)
	//		{
	//			auto& dstModulePropertyMap = dstEntityMap[entityID].ModulePropertyMap;
	//			for (auto& [propertyName, prop] : srcPropertyMap)
	//			{
	//				TOAST_CORE_ASSERT(dstModulePropertyMap.find(moduleName) != dstModulePropertyMap.end(), "");
	//				auto& propertyMap = dstModulePropertyMap.at(moduleName);
	//				TOAST_CORE_ASSERT(propertyMap.find(propertyName) != propertyMap.end(), "");
	//				propertyMap.at(propertyName).SetStoredValueRaw(prop.mStoredValueBuffer);
	//			}
	//		}
	//	}

	//}

	//void ScriptEngine::OnCreateEntity(Entity entity)
	//{
	//	EntityInstance& entityInstance = GetEntityInstanceData(entity.mScene->GetUUID(), entity.GetComponent<IDComponent>().ID).Instance;
	//	if (entityInstance.ScriptClass->OnCreateMethod)
	//		CallMethod(entityInstance.GetInstance(), entityInstance.ScriptClass->OnCreateMethod);
	//}

	//void ScriptEngine::OnUpdateEntity(UUID sceneID, UUID entityID, Timestep ts)
	//{
	//	EntityInstance& entityInstance = GetEntityInstanceData(sceneID, entityID).Instance;
	//	if (entityInstance.ScriptClass->OnClickMethod) 
	//	{
	//		void* args[] = { &ts };
	//		CallMethod(entityInstance.GetInstance(), entityInstance.ScriptClass->OnUpdateMethod, args);
	//	}
	//}

	//void ScriptEngine::OnClickEntity(Entity entity)
	//{
	//	EntityInstance& entityInstance = GetEntityInstanceData(entity.mScene->GetUUID(), entity.GetComponent<IDComponent>().ID).Instance;
	//	if (entityInstance.ScriptClass->OnCreateMethod)
	//		CallMethod(entityInstance.GetInstance(), entityInstance.ScriptClass->OnClickMethod);
	//}

	//bool ScriptEngine::ModuleExists(const std::string& moduleName)
	//{
	//	std::string NamespaceName, ClassName;
	//	if (moduleName.find('.') != std::string::npos)
	//	{
	//		NamespaceName = moduleName.substr(0, moduleName.find_last_of('.'));
	//		ClassName = moduleName.substr(moduleName.find_last_of('.') + 1);
	//	}
	//	else
	//		ClassName = moduleName;

	//	MonoClass* monoClass = mono_class_from_name(sAppAssemblyImage, NamespaceName.c_str(), ClassName.c_str());

	//	return monoClass != nullptr;
	//}

	//static PropertyType GetToastPropertyType(MonoType* monoType) 
	//{
	//	int type = mono_type_get_type(monoType);
	//	
	//	switch (type) 
	//	{
	//		case MONO_TYPE_R4: return PropertyType::Float;
	//	}
	//	return PropertyType::None;
	//}

	//void ScriptEngine::InitScriptEntity(Entity entity)
	//{
	//	Scene* scene = entity.mScene;
	//	UUID id = entity.GetComponent<IDComponent>().ID;
	//	auto& moduleName = entity.GetComponent<ScriptComponent>().ModuleName;
	//	if (moduleName.empty())
	//		return;

	//	if (!ModuleExists(moduleName))
	//	{
	//		TOAST_CORE_ERROR("Entity references non-existent script module '{0}'", moduleName);
	//		return;
	//	}

	//	EntityScriptClass& scriptClass = sEntityClassMap[moduleName];
	//	scriptClass.FullName = moduleName;
	//	if (moduleName.find('.') != std::string::npos)
	//	{
	//		scriptClass.NamespaceName = moduleName.substr(0, moduleName.find_last_of('.'));
	//		scriptClass.ClassName = moduleName.substr(moduleName.find_last_of('.') + 1);
	//	}
	//	else
	//		scriptClass.ClassName = moduleName;

	//	scriptClass.Class = GetClass(sAppAssemblyImage, scriptClass);
	//	scriptClass.InitClassMethods(sAppAssemblyImage);

	//	EntityInstanceData& entityInstanceData = sEntityInstanceMap[entity.mScene->GetUUID()][id];
	//	EntityInstance& entityInstance = entityInstanceData.Instance;
	//	entityInstance.ScriptClass = &scriptClass;
	//	ScriptModulePropertyMap& modulePropertyMap = entityInstanceData.ModulePropertyMap;
	//	auto& propertyMap = modulePropertyMap[moduleName];

	//	// Saving the old fields
	//	std::unordered_map<std::string, PublicProperty> oldProperties;
	//	oldProperties.reserve(propertyMap.size());
	//	for (auto& [propertyName, prop] : propertyMap)
	//		oldProperties.emplace(propertyName, std::move(prop));
	//	propertyMap.clear();

	//	// Retrieve the public properties
	//	{
	//		MonoClassField* iter;
	//		void* ptr = 0;
	//		while ((iter = mono_class_get_fields(scriptClass.Class, &ptr)) != NULL)
	//		{
	//			const char* name = mono_field_get_name(iter);
	//			uint32_t flags = mono_field_get_flags(iter);
	//			if ((flags & MONO_FIELD_ATTR_PUBLIC) == 0)
	//				continue;

	//			MonoType* propertyType = mono_field_get_type(iter);
	//			PropertyType toastPropertyType = GetToastPropertyType(propertyType);

	//			if (oldProperties.find(name) != oldProperties.end())
	//				propertyMap.emplace(name, std::move(oldProperties.at(name)));
	//			else
	//			{
	//				PublicProperty prop = { name, toastPropertyType };
	//				prop.mEntityInstance = &entityInstance;
	//				prop.mMonoClassField = iter;
	//				propertyMap.emplace(name, std::move(prop));
	//			}
	//		}
	//	}
	//}

	//void ScriptEngine::ShutdownScriptEntity(UUID sceneID, UUID entityID, const std::string& moduleName)
	//{
	//	EntityInstanceData& entityInstanceData = GetEntityInstanceData(sceneID, entityID);

	//	//Enter stuff to remove field map in the future
	//}

	//void ScriptEngine::InstantiateEntityClass(Entity entity)
	//{
	//	Scene* scene = entity.mScene;
	//	UUID id = entity.GetComponent<IDComponent>().ID;
	//	auto& moduleName = entity.GetComponent<ScriptComponent>().ModuleName;

	//	EntityInstanceData& entityInstanceData = GetEntityInstanceData(entity.mScene->GetUUID(), id);
	//	EntityInstance& entityInstance = entityInstanceData.Instance;
	//	TOAST_CORE_ASSERT(entityInstance.ScriptClass, "Script class in Entity Instance null");
	//	entityInstance.Handle = Instantiate(*entityInstance.ScriptClass);

	//	MonoProperty* entityIDPropery = mono_class_get_property_from_name(entityInstance.ScriptClass->Class, "ID");
	//	mono_property_get_get_method(entityIDPropery);
	//	MonoMethod* entityIDSetMethod = mono_property_get_set_method(entityIDPropery);
	//	void* param[] = { &id };
	//	CallMethod(entityInstance.GetInstance(), entityIDSetMethod, param);

	//	// Set properties to values
	//	ScriptModulePropertyMap& modulePropertyMap = entityInstanceData.ModulePropertyMap;
	//	if (modulePropertyMap.find(moduleName) != modulePropertyMap.end())
	//	{
	//		auto& publicProperties = modulePropertyMap.at(moduleName);
	//		for (auto& [name, prop] : publicProperties)
	//			prop.CopyStoredValueToRuntime();
	//	}

	//	OnCreateEntity(entity);
	//}

	//Toast::EntityInstanceMap& ScriptEngine::GetEntityInstanceMap()
	//{
	//	return sEntityInstanceMap;
	//}

	void ScriptEngine::Init()
	{
		sData = new ScriptEngineData();

		InitMono();
	}

	void ScriptEngine::Shutdown()
	{
		ShutdownMono();

		delete sData;
	}

//Toast::EntityInstanceData& ScriptEngine::GetEntityInstanceData(UUID sceneID, UUID entityID)
	//{
	//	TOAST_CORE_ASSERT(sEntityInstanceMap.find(sceneID) != sEntityInstanceMap.end(), "Invalid Scene Name!");
	//	auto& entityIDMap = sEntityInstanceMap.at(sceneID);
	//	TOAST_CORE_ASSERT(entityIDMap.find(entityID) != entityIDMap.end(), "Invalid entity ID!");
	//	return entityIDMap.at(entityID);
	//}

	char* ReadBytes(const std::string& filepath, uint32_t* outSize)
	{
		std::ifstream stream(filepath, std::ios::binary | std::ios::ate);

		if (!stream)
		{
			// Failed to open the file
			return nullptr;
		}

		std::streampos end = stream.tellg();
		stream.seekg(0, std::ios::beg);
		uint32_t size = end - stream.tellg();

		// Check if file is empty
		if (size == 0) 
			return nullptr;

		char* buffer = new char[size];
		stream.read((char*)buffer, size);
		stream.close();

		*outSize = size;
		return buffer;
	}

	MonoAssembly* LoadCSharpAssembly(const std::string& assemblyPath) 
	{
		uint32_t fileSize = 0;
		char* fileData = ReadBytes(assemblyPath, &fileSize);

		// NOTE: We can't use this image for anything other than loading the assembly because this image doesn't have a reference to the assembly
		MonoImageOpenStatus status;
		MonoImage* image = mono_image_open_from_data_full(fileData, fileSize, 1, &status, 0);

		if (status != MONO_IMAGE_OK)
		{
			const char* errorMessage = mono_image_strerror(status);
			return nullptr;
		}

		MonoAssembly* assembly = mono_assembly_load_from_full(image, assemblyPath.c_str(), &status, 0);
		mono_image_close(image);

		delete[] fileData;

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

	void ScriptEngine::InitMono()
	{
		mono_set_assemblies_path("mono/lib");

		// Create the root domain
		MonoDomain* rootDomain = mono_jit_init("ToastJITRuntime");
		TOAST_CORE_ASSERT(rootDomain, "Root domain is a nullptr");

		sData->RootDomain = rootDomain;

		// Create the app domain
		sData->AppDomain = mono_domain_create_appdomain("ToastScriptRuntime", nullptr);
		mono_domain_set(sData->AppDomain, true);

		sData->CoreAssembly = LoadCSharpAssembly("assets/Scripts/Toast-ScriptCore.dll");
		
		PrintAssemblyTypes(sData->CoreAssembly);

		MonoImage* assemblyImage = mono_assembly_get_image(sData->CoreAssembly);
		MonoClass* monoClass = mono_class_from_name(assemblyImage, "Toast", "Main");

		// 1. Create and object (and call constructor)
		MonoObject* instance = mono_object_new(sData->AppDomain, monoClass);
		mono_runtime_object_init(instance);

		// 2. Call function
		MonoMethod* printMessageFunc = mono_class_get_method_from_name(monoClass, "PrintMessage", 0);
		mono_runtime_invoke(printMessageFunc, instance, nullptr, nullptr);

		// 3. Call function with param
		MonoMethod* printIntFunc = mono_class_get_method_from_name(monoClass, "PrintInt", 1);

		int value = 5;
		void* param = &value;
		void* params[1] = 
		{
			&value
		};

		mono_runtime_invoke(printIntFunc, instance, &param, nullptr);

		MonoString* monoString = mono_string_new(sData->AppDomain, "Hello World from C++!");
		MonoMethod* printCustomMessageFunc = mono_class_get_method_from_name(monoClass, "PrintCustomMessage", 1);
		void* stringParam = monoString;
		mono_runtime_invoke(printCustomMessageFunc, instance, &stringParam, nullptr);
	}

	void ScriptEngine::ShutdownMono()
	{
		mono_domain_unload(sData->AppDomain);
		sData->AppDomain = nullptr;

		mono_jit_cleanup(sData->RootDomain);
		sData->RootDomain = nullptr;
	}

	//static uint32_t GetPropertySize(PropertyType type)
	//{
	//	switch (type) 
	//	{
	//		case PropertyType::Float:		return 4;
	//	}
	//	TOAST_CORE_ASSERT(false, "Unknown property type");

	//	return 0;
	//}

	//PublicProperty::PublicProperty(const std::string& name, PropertyType type)
	//	: Name(name), Type(type) 
	//{
	//	mStoredValueBuffer = AllocateBuffer(type);
	//}

	//PublicProperty::PublicProperty(PublicProperty&& other) 
	//{
	//	Name = std::move(other.Name);
	//	Type = other.Type;
	//	mEntityInstance = other.mEntityInstance;
	//	mMonoClassField = other.mMonoClassField;
	//	mStoredValueBuffer = other.mStoredValueBuffer;

	//	other.mEntityInstance = nullptr;
	//	other.mMonoClassField = nullptr;
	//	other.mStoredValueBuffer = nullptr;
	//}

	//PublicProperty::~PublicProperty() 
	//{
	//	delete[] mStoredValueBuffer;
	//}

	//void PublicProperty::CopyStoredValueToRuntime()
	//{
	//	TOAST_CORE_ASSERT(mEntityInstance->GetInstance(), "");
	//	mono_field_set_value(mEntityInstance->GetInstance(), mMonoClassField, mStoredValueBuffer);
	//}

	//bool PublicProperty::IsRuntimeAvailable() const
	//{
	//	return mEntityInstance->Handle != 0;
	//}

	//void PublicProperty::SetStoredValueRaw(void* src)
	//{
	//	uint32_t size = GetPropertySize(Type);
	//	memcpy(mStoredValueBuffer, src, size);
	//}

	//uint8_t* PublicProperty::AllocateBuffer(PropertyType type)
	//{
	//	uint32_t size = GetPropertySize(type);
	//	uint8_t* buffer = new uint8_t[size];
	//	memset(buffer, 0, size);
	//	return buffer;
	//}

	//void PublicProperty::SetStoredValue_Internal(void* value) const
	//{
	//	uint32_t size = GetPropertySize(Type);
	//	memcpy(mStoredValueBuffer, value, size);
	//}

	//void PublicProperty::GetStoredValue_Internal(void* outValue) const
	//{
	//	uint32_t size = GetPropertySize(Type);
	//	memcpy(outValue, mStoredValueBuffer, size);
	//}

	//void PublicProperty::SetRuntimeValue_Internal(void* value) const
	//{
	//	TOAST_CORE_ASSERT(mEntityInstance->GetInstance(), "");
	//	mono_field_set_value(mEntityInstance->GetInstance(), mMonoClassField, value);
	//}

	//void PublicProperty::GetRuntimeValue_Internal(void* outValue) const
	//{
	//	TOAST_CORE_ASSERT(mEntityInstance->GetInstance(), "");
	//	mono_field_get_value(mEntityInstance->GetInstance(), mMonoClassField, outValue);
	//}

}
