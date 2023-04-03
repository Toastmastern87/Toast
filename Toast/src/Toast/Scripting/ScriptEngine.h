#pragma once

#include <string>

#include "Toast/Scene/Scene.h"
#include "Toast/Scene/Entity.h"

extern "C" {
	typedef struct _MonoImage MonoImage;
	typedef struct _MonoClass MonoClass;
	typedef struct _MonoMethod MonoMethod;
	typedef struct _MonoObject MonoObject;
	typedef struct _MonoAssembly MonoAssembly;
	typedef struct _MonoClassField MonoClassField;
}

namespace Toast {

	enum class ScriptFieldType
	{
		None = 0,
		Float, Double,
		Byte, Char, Short, Int, Long, Bool,
		UByte, UShort, UInt, ULong,
		Vector2, Vector3, Vector4,
		Entity
	};

	struct ScriptField
	{
		std::string Name;
		ScriptFieldType Type;

		MonoClassField* ClassField;
	};

	// ScriptField + data storage
	struct ScriptFieldInstance
	{
		ScriptField Field;

		ScriptFieldInstance()
		{
			memset(mBuffer, 0, sizeof(mBuffer));
		}

		template<typename T>
		T GetValue()
		{
			static_assert(sizeof(T) <= 16, "Type to large!");
			return *(T*)mBuffer;
		}

		template<typename T>
		void SetValue(T value)
		{
			static_assert(sizeof(T) <= 16, "Type to large!");
			memcpy(mBuffer, &value, sizeof(T));

		}

	private:
		uint8_t mBuffer[16];

		friend class ScriptEngine;
		friend class ScriptInstance;
	};

	using ScriptFieldMap = std::unordered_map<std::string, ScriptFieldInstance>;

	class ScriptClass
	{
	public:
		ScriptClass() = default;
		ScriptClass(const std::string& classNamespace, const std::string& className, bool isCore = false);

		MonoObject* Instantiate();
		MonoMethod* GetMethod(const std::string& name, int parameterCount);
		MonoObject* InvokeMethod(MonoObject* instance, MonoMethod* method, void** params = nullptr);

		const std::unordered_map<std::string, ScriptField>& GetFields() const { return mFields; }
	private:
		std::string mClassNamespace;
		std::string mClassName;

		std::unordered_map<std::string, ScriptField> mFields;

		MonoClass* mMonoClass = nullptr;

		friend class ScriptEngine;
	};

	class ScriptInstance
	{
	public:
		ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity);

		void InvokeOnCreate();
		void InvokeOnUpdate(float ts);
		void InvokeOnEvent();

		Ref<ScriptClass> GetScriptClass() { return mScriptClass; }

		template<typename T>
		T GetFieldValue(const std::string& name)
		{
			static_assert(sizeof(T) <= 16, "Type to large!");

			bool success = GetFieldValueInternal(name, sFieldValueBuffer);
			if (!success)
				return T();

			return *(T*)sFieldValueBuffer;
		}

		template<typename T>
		void SetFieldValue(const std::string& name, T value)
		{
			static_assert(sizeof(T) <= 16, "Type to large!");

			SetFieldValueInternal(name, &value);
		}

		MonoObject* GetManagedObject() { return mInstance; }
	private:
		bool GetFieldValueInternal(const std::string& name, void* buffer);
		bool SetFieldValueInternal(const std::string& name, const void* value);
	private:
		Ref<ScriptClass> mScriptClass;

		MonoObject* mInstance = nullptr;
		MonoMethod* mConstructor = nullptr;
		MonoMethod* mOnCreateMethod = nullptr;
		MonoMethod* mOnUpdateMethod = nullptr;
		MonoMethod* mOnEventMethod = nullptr;

		inline static char sFieldValueBuffer[16];

		friend class ScriptEngine;
		friend struct ScriptFieldInstance;
	};

	class ScriptEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static bool LoadAssembly(const std::filesystem::path& filepath);
		static bool LoadAppAssembly(const std::filesystem::path& filepath);

		static void ReloadAssembly();

		static void OnRuntimeStart(Scene* scene);
		static void OnRuntimeStop();

		static bool EntityClassExists(const std::string& fullClassName);
		static void OnCreateEntity(Entity entity);
		static void OnUpdateEntity(Entity entity, Timestep ts);
		static void OnEventEntity(Entity entity);

		static Scene* GetSceneContext();
		static Ref<ScriptInstance> GetEntityScriptInstance(UUID entityID);

		static Ref<ScriptClass> GetEntityClass(const std::string& name);
		static std::unordered_map<std::string, Ref<ScriptClass>> GetEntityClasses();
		static ScriptFieldMap& GetScriptFieldMap(Entity entity);

		static MonoImage* GetCoreAssemblyImage();

		static MonoObject* GetManagedInstance(UUID uuid);
	private:
		static void InitMono();
		static void ShutdownMono();

		static MonoObject* InstantiateClass(MonoClass* monoClass);
		static void LoadAssemblyClasses();

		friend class ScriptClass;
		friend class ScriptGlue;
	};

	namespace Utils {

		inline const char* ScriptFieldTypeToString(ScriptFieldType fieldType)
		{
			switch (fieldType)
			{
			case ScriptFieldType::None:	return "None";
			case ScriptFieldType::Float:	return "Float";
			case ScriptFieldType::Double:	return "Double";
			case ScriptFieldType::Bool:		return "Bool";
			case ScriptFieldType::Char:		return "Char";
			case ScriptFieldType::Byte:		return "Byte";
			case ScriptFieldType::Short:	return "Short";
			case ScriptFieldType::Int:		return "Int";
			case ScriptFieldType::Long:		return "Long";
			case ScriptFieldType::UByte:	return "UByte";
			case ScriptFieldType::UShort:	return "UShort";
			case ScriptFieldType::UInt:		return "UInt";
			case ScriptFieldType::ULong:	return "ULong";
			case ScriptFieldType::Vector2:	return "Vector2";
			case ScriptFieldType::Vector3:	return "Vector3";
			case ScriptFieldType::Vector4:	return "Vector4";
			case ScriptFieldType::Entity:	return "Entity";
			}

			TOAST_CORE_ASSERT(false, "Unknown ScriptFieldType");
			return "None";
		}

		inline ScriptFieldType ScriptFieldTypeFromString(std::string_view fieldType)
		{
			if (fieldType == "None")	return ScriptFieldType::None;
			if (fieldType == "Float")	return ScriptFieldType::Float;
			if (fieldType == "Double")	return ScriptFieldType::Double;
			if (fieldType == "Bool")	return ScriptFieldType::Bool;
			if (fieldType == "Char")	return ScriptFieldType::Char;
			if (fieldType == "Byte")	return ScriptFieldType::Byte;
			if (fieldType == "Short")	return ScriptFieldType::Short;
			if (fieldType == "Int")		return ScriptFieldType::Int;
			if (fieldType == "Long")	return ScriptFieldType::Long;
			if (fieldType == "UByte")	return ScriptFieldType::UByte;
			if (fieldType == "UShort")	return ScriptFieldType::UShort;
			if (fieldType == "UInt")	return ScriptFieldType::UInt;
			if (fieldType == "ULong")	return ScriptFieldType::ULong;
			if (fieldType == "Vector2") return ScriptFieldType::Vector2;
			if (fieldType == "Vector3") return ScriptFieldType::Vector3;
			if (fieldType == "Vector4") return ScriptFieldType::Vector4;
			if (fieldType == "Entity")	return ScriptFieldType::Entity;

			TOAST_CORE_ASSERT(false, "Unknown ScriptFieldType");
			return ScriptFieldType::None;
		}
	}
}