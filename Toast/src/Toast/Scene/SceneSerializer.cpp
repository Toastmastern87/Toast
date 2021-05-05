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
			node.SetStyle(EmitterStyle::Flow);
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
			node.SetStyle(EmitterStyle::Flow);
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
		UUID uuid = entity.GetComponent<IDComponent>().ID;
		out << YAML::BeginMap; // Entity
		out << YAML::Key << "Entity" << YAML::Value << uuid; 

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
			DirectX::XMFLOAT3 translationFloat3, scaleFloat3;
			DirectX::XMVECTOR translation, scale, rotation;

			DirectX::XMMatrixDecompose(&scale, &rotation, &translation, tc.Transform);
			DirectX::XMStoreFloat3(&translationFloat3, translation);
			DirectX::XMStoreFloat3(&scaleFloat3, scale);

			out << YAML::Key << "Translation" << YAML::Value << translationFloat3;
			out << YAML::Key << "Rotation" << YAML::Value << tc.RotationEulerAngles;
			out << YAML::Key << "Scale" << YAML::Value << scaleFloat3;

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

		if (entity.HasComponent<SkyLightComponent>())
		{
			out << YAML::Key << "SkyLightComponent";
			out << YAML::BeginMap; // SkyLightComponent

			auto& skc = entity.GetComponent<SkyLightComponent>();
			out << YAML::Key << "AssetPath" << YAML::Value << skc.SceneEnvironment.FilePath;
			out << YAML::Key << "Intensity" << YAML::Value << skc.Intensity;

			out << YAML::EndMap; // SkyLightComponent
		}

		if (entity.HasComponent<DirectionalLightComponent>())
		{
			out << YAML::Key << "DirectionalLightComponent";
			out << YAML::BeginMap; // DirectionalLightComponent

			auto& dlc = entity.GetComponent<DirectionalLightComponent>();
			out << YAML::Key << "Radiance" << YAML::Value << dlc.Radiance;
			out << YAML::Key << "Intensity" << YAML::Value << dlc.Intensity;
			out << YAML::Key << "SunDisk" << YAML::Value << dlc.SunDisc;

			out << YAML::EndMap; // SkyLightComponent
		}

		if (entity.HasComponent<ScriptComponent>())
		{
			out << YAML::Key << "ScriptComponent";
			out << YAML::BeginMap; // ScriptComponent

			auto& sc = entity.GetComponent<ScriptComponent>();
			out << YAML::Key << "AssetPath" << YAML::Value << sc.ModuleName;

			out << YAML::EndMap; // ScriptComponent
		}

		out << YAML::EndMap; // Entity
	}

	void SceneSerializer::Serialize(const std::string& filepath)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		// TODO, should be scene name instead of just untitled scene
		out << YAML::Key << "Scene" << YAML::Value << "Untitled Scene";
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
		mScene->mRegistry.each([&](auto entityID)
		{
			Entity entity = { entityID, mScene.get() };
			if (!entity || !entity.HasComponent<IDComponent>())
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
		YAML::Node data = YAML::LoadFile(filepath);
		if (!data["Scene"])
			return false;

		std::string sceneName = data["Scene"].as<std::string>();
		TOAST_CORE_TRACE("Deserializing scene '%s'", sceneName.c_str());

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

				TOAST_CORE_TRACE("Deserialized entity with ID '%llu', name '%s'", uuid, name.c_str());

				Entity deserializedEntity = mScene->CreateEntityWithID(uuid, name);

				auto transformComponent = entity["TransformComponent"];
				if (transformComponent) 
				{
					// Entities always have transforms
					auto& tc = deserializedEntity.GetComponent<TransformComponent>();
					DirectX::XMFLOAT3 translation = transformComponent["Translation"].as<DirectX::XMFLOAT3>();
					DirectX::XMFLOAT3 rotation = transformComponent["Rotation"].as<DirectX::XMFLOAT3>();
					DirectX::XMFLOAT3 scale = transformComponent["Scale"].as<DirectX::XMFLOAT3>();

					tc.RotationEulerAngles = rotation;
					tc.Transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(scale.x, scale.y, scale.z)
						* (DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(rotation.x), DirectX::XMConvertToRadians(rotation.y), DirectX::XMConvertToRadians(rotation.z))))
						* DirectX::XMMatrixTranslation(translation.x, translation.y, translation.z);
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

				auto skylightComponent = entity["SkyLightComponent"];
				if (skylightComponent)
				{
					auto& skc = deserializedEntity.AddComponent<SkyLightComponent>();
					
					skc.SceneEnvironment = Environment::Load(skylightComponent["AssetPath"].as<std::string>());
					skc.Intensity = skylightComponent["Intensity"].as<float>();
				}

				auto directionalLightComponent = entity["DirectionalLightComponent"];
				if (directionalLightComponent)
				{
					auto& dlc = deserializedEntity.AddComponent<DirectionalLightComponent>();

					dlc.Radiance = directionalLightComponent["Radiance"].as<DirectX::XMFLOAT3>();
					dlc.Intensity = directionalLightComponent["Intensity"].as<float>();
					dlc.SunDisc = directionalLightComponent["SunDisk"].as<bool>();
				}

				auto scriptComponent = entity["ScriptComponent"];
				if (scriptComponent)
				{
					std::string moduleName = scriptComponent["AssetPath"].as<std::string>();
					auto& sc = deserializedEntity.AddComponent<ScriptComponent>(moduleName);
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