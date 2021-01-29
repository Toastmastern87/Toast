#include "tpch.h"
#include "SceneSerializer.h"

#include "Entity.h"
#include "Components.h"

#include <yaml-cpp/yaml.h>

namespace YAML 
{
	template<>
	struct convert<DirectX::XMFLOAT3>
	{
		static Node encode(const DirectX::XMFLOAT3& rhs) 
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			return node;
		}

		static bool decode(const Node& node, DirectX::XMFLOAT3& rhs) 
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			return true;
		}
	};

	template<>
	struct convert<DirectX::XMFLOAT4>
	{
		static Node encode(const DirectX::XMFLOAT4& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			return node;
		}

		static bool decode(const Node& node, DirectX::XMFLOAT4& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			rhs.w = node[3].as<float>();
			return true;
		}
	};
}

namespace Toast {

	YAML::Emitter& operator<<(YAML::Emitter& out, const DirectX::XMFLOAT3& v) 
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
		return out;
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const DirectX::XMFLOAT4& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
		return out;
	}

	SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
		: mScene(scene)
	{

	}

	static void SerializeEntity(YAML::Emitter& out, Entity entity)
	{
		out << YAML::BeginMap; // Entity
		out << YAML::Key << "Entity" << YAML::Value << "12837192831273"; // TODO: Entity ID goes here

		if (entity.HasComponent<TagComponent>()) 
		{
			out << YAML::Key << "TagComponent";
			out << YAML::BeginMap; // TagComponent

			auto& tag = entity.GetComponent<TagComponent>().Tag;
			out << YAML::Key << "Tag" << YAML::Value << tag; 

			out << YAML::EndMap; // TagComponent
		}

		if (entity.HasComponent<TransformComponent>())
		{
			out << YAML::Key << "TransformComponent";
			out << YAML::BeginMap; // TransformComponent

			auto& tc = entity.GetComponent<TransformComponent>();
			out << YAML::Key << "Translation" << YAML::Value << tc.Translation;
			out << YAML::Key << "Rotation" << YAML::Value << tc.Rotation;
			out << YAML::Key << "Scale" << YAML::Value << tc.Scale;

			out << YAML::EndMap; // TransformComponent
		}

		if (entity.HasComponent<CameraComponent>())
		{
			out << YAML::Key << "CameraComponent";
			out << YAML::BeginMap; // CameraComponent

			auto& cc = entity.GetComponent<CameraComponent>();
			auto& camera = cc.Camera;

			out << YAML::Key << "Camera" << YAML::Value;
			out << YAML::BeginMap; // Camera
			out << YAML::Key << "ProjectionType" << YAML::Value << (int)camera.GetProjectionType();
			out << YAML::Key << "PerspectiveFOV" << YAML::Value << camera.GetPerspectiveVerticalFOV();
			out << YAML::Key << "PerspectiveNear" << YAML::Value << camera.GetPerspectiveNearClip();
			out << YAML::Key << "PerspectiveFar" << YAML::Value << camera.GetPerspectiveFarClip();
			out << YAML::Key << "OrthographicSize" << YAML::Value << camera.GetOrthographicSize();
			out << YAML::Key << "OrthographicNear" << YAML::Value << camera.GetOrthographicNearClip();
			out << YAML::Key << "OrthographicFar" << YAML::Value << camera.GetOrthographicFarClip();
			out << YAML::EndMap; // Camera

			out << YAML::Key << "Primary" << YAML::Value << cc.Primary;
			out << YAML::Key << "FixedAspectRatio" << YAML::Value << cc.FixedAspectRatio;

			out << YAML::EndMap; // CameraComponent
		}

		if (entity.HasComponent<MeshComponent>())
		{
			out << YAML::Key << "MeshComponent";
			out << YAML::BeginMap; // MeshComponent

			auto& pmc = entity.GetComponent<MeshComponent>();
			out << YAML::Key << "Material" << YAML::Value << pmc.Mesh->GetMaterial()->GetName();

			out << YAML::EndMap; // MeshComponent
		}

		if (entity.HasComponent<SpriteRendererComponent>())
		{
			out << YAML::Key << "SpriteRendererComponent";
			out << YAML::BeginMap; // SpriteRendererComponent

			auto& src = entity.GetComponent<SpriteRendererComponent>();
			out << YAML::Key << "Color" << YAML::Value << src.Color;

			out << YAML::EndMap; // SpriteRendererComponent
		}

		if (entity.HasComponent<PlanetComponent>())
		{
			out << YAML::Key << "PlanetComponent";
			out << YAML::BeginMap; // PlanetComponent

			auto& pc = entity.GetComponent<PlanetComponent>();
			out << YAML::Key << "Subdivisions" << YAML::Value << pc.Subdivisions;
			out << YAML::Key << "PatchLevels" << YAML::Value << pc.PatchLevels;
			out << YAML::Key << "MaxAltitude" << YAML::Value << pc.PlanetData.maxAltitude;
			out << YAML::Key << "MinAltitude" << YAML::Value << pc.PlanetData.minAltitude;
			out << YAML::Key << "Radius" << YAML::Value << pc.PlanetData.radius;

			out << YAML::EndMap; // PlanetComponent
		}

		out << YAML::EndMap; // Entity
	}

	static void SerializeEnvironment(YAML::Emitter& out, const Ref<Scene>& scene)
	{
		out << YAML::Key << "Environment";
		out << YAML::Value;
		out << YAML::BeginMap; // Environment
		out << YAML::Key << "AssetPath" << YAML::Value << scene->GetEnvironment().FilePath;
		out << YAML::Key << "EnvironmentIntensity" << YAML::Value << scene->GetEnvironmentIntensity();
		out << YAML::Key << "TextureLOD" << YAML::Value << scene->GetSkyboxLod();
		out << YAML::EndMap; // Environment
	}


	void SceneSerializer::Serialize(const std::string& filepath)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		// TODO, should be scene name instead of just untitled scene
		out << YAML::Key << "Scene" << YAML::Value << "Untitled Scene";
		SerializeEnvironment(out, mScene);
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
		mScene->mRegistry.each([&](auto entityID)
		{
			Entity entity = { entityID, mScene.get() };
			if (!entity)
				return;

			SerializeEntity(out, entity);
		});
		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::ofstream fout(filepath);
		fout << out.c_str();
	}

	void SceneSerializer::SerializeRuntime(const std::string& filepath)
	{
		// Not implemented yet
		TOAST_CORE_ASSERT(false, "Not implemented yet!");
	}

	bool SceneSerializer::Deserialize(const std::string& filepath)
	{
		std::ifstream stream(filepath);
		std::stringstream strStream;

		strStream << stream.rdbuf();

		YAML::Node data = YAML::Load(strStream.str());
		if (!data["Scene"])
			return false;

		std::string sceneName = data["Scene"].as<std::string>();
		TOAST_CORE_TRACE("Deserializing scene '{0}'", sceneName);

		auto environment = data["Environment"];
		if (environment)
		{
			std::string envPath = environment["AssetPath"].as<std::string>();
			mScene->SetEnvironment(Environment::Load(envPath));

			mScene->GetEnvironmentIntensity() = environment["EnvironmentIntensity"].as<float>();
			mScene->GetSkyboxLod() = environment["TextureLOD"].as<float>();
		}

		auto entities = data["Entities"];
		if (entities) 
		{
			for (auto entity : entities) 
			{
				uint64_t uuid = entity["Entity"].as<uint64_t>(); // TODO
				
				std::string name;
				auto tagComponent = entity["TagComponent"];
				if (tagComponent)
					name = tagComponent["Tag"].as<std::string>();

				TOAST_CORE_TRACE("Deserialized entity with ID = {0}, name = {1}", uuid, name);

				Entity deserializedEntity = mScene->CreateEntity(name);

				auto transformComponent = entity["TransformComponent"];
				if (transformComponent) 
				{
					// Entities always have transforms
					auto& tc = deserializedEntity.GetComponent<TransformComponent>();
					tc.Translation = transformComponent["Translation"].as<DirectX::XMFLOAT3>();
					tc.Rotation = transformComponent["Rotation"].as<DirectX::XMFLOAT3>();
					tc.Scale = transformComponent["Scale"].as<DirectX::XMFLOAT3>();
				}

				auto cameraComponent = entity["CameraComponent"];
				if (cameraComponent)
				{
					auto& cc = deserializedEntity.AddComponent<CameraComponent>();

					auto& cameraProps = cameraComponent["Camera"];
					cc.Camera.SetProjectionType((SceneCamera::ProjectionType)cameraProps["ProjectionType"].as<int>());

					cc.Camera.SetPerspectiveVerticalFOV(cameraProps["PerspectiveFOV"].as<float>());
					cc.Camera.SetPerspectiveNearClip(cameraProps["PerspectiveNear"].as<float>());
					cc.Camera.SetPerspectiveFarClip(cameraProps["PerspectiveFar"].as<float>());

					cc.Camera.SetOrthographicSize(cameraProps["OrthographicSize"].as<float>());
					cc.Camera.SetOrthographicNearClip(cameraProps["OrthographicNear"].as<float>());
					cc.Camera.SetOrthographicFarClip(cameraProps["OrthographicFar"].as<float>());

					cc.Primary = cameraComponent["Primary"].as<bool>();
					cc.FixedAspectRatio = cameraComponent["FixedAspectRatio"].as<bool>();
				}

				auto meshComponent = entity["PrimitiveMeshComponent"];
				if (meshComponent)
				{
					deserializedEntity.AddComponent<MeshComponent>(CreateRef<Mesh>());
					auto& mc = deserializedEntity.GetComponent<MeshComponent>();

					mc.Mesh->SetMaterial(MaterialLibrary::Get(meshComponent["Material"].as<std::string>()));
				}

				auto spriteRendererComponent = entity["SpriteRendererComponent"];
				if (spriteRendererComponent)
				{
					auto& src = deserializedEntity.AddComponent<SpriteRendererComponent>();
					src.Color = spriteRendererComponent["Color"].as<DirectX::XMFLOAT4>();
				}

				auto planetComponent = entity["PlanetComponent"];
				if (planetComponent) 
				{
					auto& pc = deserializedEntity.AddComponent<PlanetComponent>(planetComponent["Subdivisions"].as<int16_t>(), planetComponent["PatchLevels"].as<int16_t>(), planetComponent["MaxAltitude"].as<DirectX::XMFLOAT4>(), planetComponent["MinAltitude"].as<DirectX::XMFLOAT4>(), planetComponent["Radius"].as<DirectX::XMFLOAT4>());
				}
			}
		}

		return true;
	}

	bool SceneSerializer::DeserializeRuntime(const std::string& filepath)
	{
		// Not implemented yet
		TOAST_CORE_ASSERT(false, "Not implemented yet!");
		return false;
	}

}