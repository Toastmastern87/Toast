#pragma once

#include "Scene.h"

#include <yaml-cpp/yaml.h>

namespace Toast {

#define WRITE_SCRIPT_FIELD(FieldType, Type)								\
			case ScriptFieldType::FieldType:							\
					out << scriptField.GetValue<Type>();				\
					break

#define READ_SCRIPT_FIELD(FieldType, Type)								\
				case ScriptFieldType::FieldType:						\
				{														\
					Type data = scriptField["Data"].as<Type>();			\
					fieldInstance.SetValue(data);						\
					break;												\
				}														\

	inline YAML::Emitter& operator<<(YAML::Emitter& out, const Vector3& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
		return out;
	}

	inline YAML::Emitter& operator<<(YAML::Emitter& out, const DirectX::XMFLOAT2& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
		return out;
	}

	inline YAML::Emitter& operator<<(YAML::Emitter& out, const DirectX::XMFLOAT3& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
		return out;
	}

	inline YAML::Emitter& operator<<(YAML::Emitter& out, const DirectX::XMFLOAT4& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
		return out;
	}

	inline YAML::Emitter& operator<<(YAML::Emitter& out, const DirectX::XMFLOAT3X3& m)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << m.m[0][0] << m.m[0][1] << m.m[0][2] << m.m[1][0] << m.m[1][1] << m.m[1][2] << m.m[2][0] << m.m[2][1] << m.m[2][2] << YAML::EndSeq;
		return out;
	}

	class SceneSerializer 
	{
	public:
		SceneSerializer(const Ref<Scene>& scene);

		void Serialize(const std::string& filepath);
		void SerializeRuntime(const std::string& filepath);

		bool Deserialize(const std::string& filepath);
		bool DeserializeRuntime(const std::string& filepath);

		void CopyComponents(Entity& target, Entity& source);

		void InstantiatePrefabChildren(Scene* currentScene, Entity& parentEntity, Entity prefabParent);
	private:
		Ref<Scene> mScene;
	};
}