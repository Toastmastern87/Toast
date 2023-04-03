#include "tpch.h"
#include "SceneSerializer.h"

#include "Entity.h"
#include "Components.h"

#include "Toast/Scripting/ScriptEngine.h"

#include "Toast/Physics/PhysicsEngine.h"

#include <yaml-cpp/yaml.h>

namespace YAML 
{
	template<>
	struct convert<DirectX::XMFLOAT2>
	{
		static Node encode(const DirectX::XMFLOAT2& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, DirectX::XMFLOAT2& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			return true;
		}
	};

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

	template<>
	struct convert<Toast::UUID>
	{
		static Node encode(const Toast::UUID& uuid) 
		{
			Node node;
			node.push_back((uint64_t)uuid);
			return node;
		}

		static bool decode(const Node& node, Toast::UUID& uuid)
		{
			uuid = node.as<uint64_t>();
			return true;
		}
	};

}

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

	YAML::Emitter& operator<<(YAML::Emitter& out, const DirectX::XMFLOAT2& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
		return out;
	}

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

		if (entity.HasComponent<RelationshipComponent>())
		{
			out << YAML::Key << "RelationshipComponent";
			out << YAML::BeginMap; // RelationshipComponent

			auto& rc = entity.GetComponent<RelationshipComponent>();
			out << YAML::Key << "ParentHandle" << YAML::Value << rc.ParentHandle;

			out << YAML::Key << "Children";
			out << YAML::Value << YAML::BeginSeq;

			for (auto child : rc.Children)
			{
				out << YAML::BeginMap;
				out << YAML::Key << "Handle" << YAML::Value << child;
				out << YAML::EndMap;
			}

			out << YAML::EndSeq;

			out << YAML::EndMap; // RelationshipComponent
		}

		if (entity.HasComponent<TransformComponent>())
		{
			out << YAML::Key << "TransformComponent";
			out << YAML::BeginMap; // TransformComponent

			auto& tc = entity.GetComponent<TransformComponent>();

			out << YAML::Key << "Translation" << YAML::Value << tc.Translation;
			out << YAML::Key << "Rotation" << YAML::Value << tc.RotationEulerAngles;
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
			out << YAML::Key << "NearClip" << YAML::Value << camera.GetNearClip();
			out << YAML::Key << "FarClip" << YAML::Value << camera.GetFarClip();
			out << YAML::Key << "OrthographicWidth" << YAML::Value << camera.GetOrthographicWidth();
			out << YAML::Key << "OrthographicHeight" << YAML::Value << camera.GetOrthographicHeight();
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
			if(!pmc.Mesh->GetIsPlanet())
				out << YAML::Key << "AssetPath" << YAML::Value << pmc.Mesh->GetFilePath();
			out << YAML::Key << "IsPlanet" << YAML::Value << pmc.Mesh->GetIsPlanet();
			//out << YAML::Key << "Material" << YAML::Value << pmc.Mesh->GetMaterial()->GetName();

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
			out << YAML::Key << "GravitationalAcceleration" << YAML::Value << pc.PlanetData.gravAcc;
			out << YAML::Key << "AtmosphereHeight" << YAML::Value << pc.PlanetData.atmosphereHeight;
			out << YAML::Key << "AtmosphereToggle" << YAML::Value << pc.PlanetData.atmosphereToggle;
			out << YAML::Key << "InScatteringPoints" << YAML::Value << pc.PlanetData.inScatteringPoints;
			out << YAML::Key << "OpticalDepthPoints" << YAML::Value << pc.PlanetData.opticalDepthPoints;
			out << YAML::Key << "MieAnisotropy" << YAML::Value << pc.PlanetData.mieAnisotropy;
			out << YAML::Key << "RayScaleHeight" << YAML::Value << pc.PlanetData.rayScaleHeight;
			out << YAML::Key << "MieScaleHeight" << YAML::Value << pc.PlanetData.mieScaleHeight;
			out << YAML::Key << "RayBaseScatteringCoefficient" << YAML::Value << pc.PlanetData.rayBaseScatteringCoefficient;
			out << YAML::Key << "MieBaseScatteringCoefficient" << YAML::Value << pc.PlanetData.mieBaseScatteringCoefficient;

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
			out << YAML::Key << "ClassName" << YAML::Value << sc.ClassName;

			// Fields
			Ref<ScriptClass> entityClass = ScriptEngine::GetEntityClass(sc.ClassName);
			const auto& fields = entityClass->GetFields();
			if(fields.size() > 0)
			{
				out << YAML::Key << "ScriptFields" << YAML::Value;
				auto& entityFields = ScriptEngine::GetScriptFieldMap(entity);
				out << YAML::BeginSeq;
				for (const auto& [name, field] : fields)
				{
					if (entityFields.find(name) == entityFields.end())
						continue;

					out << YAML::BeginMap; // ScriptFields

					out << YAML::Key << "Name" << YAML::Value << name;
					out << YAML::Key << "Type" << YAML::Value << Utils::ScriptFieldTypeToString(field.Type);

					out << YAML::Key << "Data" << YAML::Value;
					ScriptFieldInstance& scriptField = entityFields.at(name);

					switch (field.Type)
					{
						WRITE_SCRIPT_FIELD(Float,	float);
						WRITE_SCRIPT_FIELD(Double,	double);
						WRITE_SCRIPT_FIELD(Bool,	bool);
						WRITE_SCRIPT_FIELD(Char,	char);
						WRITE_SCRIPT_FIELD(Byte,	int8_t);
						WRITE_SCRIPT_FIELD(Short,	int16_t);
						WRITE_SCRIPT_FIELD(Int,		int32_t);
						WRITE_SCRIPT_FIELD(Long,	int64_t);
						WRITE_SCRIPT_FIELD(UByte,	uint8_t);
						WRITE_SCRIPT_FIELD(UShort,	uint16_t);
						WRITE_SCRIPT_FIELD(UInt,	uint32_t);
						WRITE_SCRIPT_FIELD(ULong,	uint64_t);
						WRITE_SCRIPT_FIELD(Vector2, DirectX::XMFLOAT2);
						WRITE_SCRIPT_FIELD(Vector3, DirectX::XMFLOAT3);
						WRITE_SCRIPT_FIELD(Vector4, DirectX::XMFLOAT4);
						WRITE_SCRIPT_FIELD(Entity,	UUID);
					}
					out << YAML::EndMap; // ScriptFields
				}
				out << YAML::EndSeq;
			}

			out << YAML::EndMap; // ScriptComponent
		}

		if (entity.HasComponent<RigidBodyComponent>())
		{
			out << YAML::Key << "RigidBodyComponent";
			out << YAML::BeginMap; // RigidBodyComponent

			auto& rbc = entity.GetComponent<RigidBodyComponent>();
			out << YAML::Key << "InvMass" << YAML::Value << rbc.InvMass;
			out << YAML::Key << "CenterOfMass" << YAML::Value << rbc.CenterOfMass;

			out << YAML::EndMap; // RigidBodyComponent
		}

		if (entity.HasComponent<SphereColliderComponent>())
		{
			out << YAML::Key << "SphereColliderComponent";
			out << YAML::BeginMap; // SphereColliderComponent

			auto& scc = entity.GetComponent<SphereColliderComponent>();
			out << YAML::Key << "RenderCollider" << YAML::Value << scc.RenderCollider;
			out << YAML::Key << "Radius" << YAML::Value << scc.Radius;

			out << YAML::EndMap; // SphereColliderComponent
		}

		if (entity.HasComponent<TerrainColliderComponent>())
		{
			out << YAML::Key << "TerrainColliderComponent";
			out << YAML::BeginMap; // TerrainColliderComponent

			auto& scc = entity.GetComponent<TerrainColliderComponent>();
			out << YAML::Key << "AssetPath" << YAML::Value << scc.FilePath;

			out << YAML::EndMap; // TerrainColliderComponent
		}

		if (entity.HasComponent<UIPanelComponent>())
		{
			out << YAML::Key << "UIPanelComponent";
			out << YAML::BeginMap; // UIPanelComponent

			auto& uipc = entity.GetComponent<UIPanelComponent>();
			out << YAML::Key << "Color" << YAML::Value << uipc.Panel->GetColorF4();
			out << YAML::Key << "CornerRadius" << YAML::Value << *uipc.Panel->GetCornerRadius();

			out << YAML::EndMap; // UIPanelComponent
		}

		if (entity.HasComponent<UIButtonComponent>())
		{
			out << YAML::Key << "UIButtonComponent";
			out << YAML::BeginMap; // UIButtonComponent

			auto& ubc = entity.GetComponent<UIButtonComponent>();
			out << YAML::Key << "CornerRadius" << YAML::Value << *ubc.Button->GetCornerRadius();
			out << YAML::Key << "Color" << YAML::Value << ubc.Button->GetColorF4();
			out << YAML::Key << "ClickColor" << YAML::Value << ubc.Button->GetClickColorF4();

			out << YAML::EndMap; // UIButtonComponent
		}

		if (entity.HasComponent<UITextComponent>())
		{
			out << YAML::Key << "UITextComponent";
			out << YAML::BeginMap; // UITextComponent

			auto& uitc = entity.GetComponent<UITextComponent>();
			out << YAML::Key << "AssetPath" << YAML::Value << uitc.Text->GetFont()->GetFilePath();
			out << YAML::Key << "Text" << YAML::Value << uitc.Text->GetText();

			out << YAML::EndMap; // UITextComponent
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
		YAML::Node data;
		try 
		{
			data = YAML::LoadFile(filepath);
		}
		catch (const YAML::ParserException& ex)
		{
			TOAST_CORE_ERROR("Failed to deserialize scene %s\n		%s", filepath.c_str(), ex.what());

			return false;
		}

		if (!data["Scene"])
			return false;

		std::string sceneName = data["Scene"].as<std::string>();
		TOAST_CORE_TRACE("Deserializing scene '%s'", sceneName.c_str());

		auto entities = data["Entities"];
		if (entities) 
		{
			for (auto entity : entities)
			{
				uint64_t uuid = entity["Entity"].as<UUID>();

				std::string name;
				auto tagComponent = entity["TagComponent"];
				if (tagComponent)
					name = tagComponent["Tag"].as<std::string>();

				TOAST_CORE_TRACE("Deserialized entity with ID '%llu', name '%s'", uuid, name.c_str());

				Entity deserializedEntity = mScene->CreateEntityWithID(uuid, name);

				auto relationshipComponent = entity["RelationshipComponent"];
				auto& rc = deserializedEntity.GetComponent<RelationshipComponent>();
				rc.ParentHandle = relationshipComponent["ParentHandle"].as<uint64_t>();
				auto children = relationshipComponent["Children"];
				if (children)
				{
					for (auto child : children)
					{
						uint64_t childHandle = child["Handle"].as<uint64_t>();
						rc.Children.push_back(childHandle);
					}
				}

				auto transformComponent = entity["TransformComponent"];
				if (transformComponent) 
				{
					// Entities always have transforms
					auto& tc = deserializedEntity.GetComponent<TransformComponent>();
					tc.Translation = transformComponent["Translation"].as<DirectX::XMFLOAT3>();
					tc.RotationEulerAngles = transformComponent["Rotation"].as<DirectX::XMFLOAT3>();
					tc.Scale = transformComponent["Scale"].as<DirectX::XMFLOAT3>();
				}

				auto cameraComponent = entity["CameraComponent"];
				if (cameraComponent)
				{
					auto& cc = deserializedEntity.AddComponent<CameraComponent>();

					auto& cameraProps = cameraComponent["Camera"];
					cc.Camera.SetProjectionType((SceneCamera::ProjectionType)cameraProps["ProjectionType"].as<int>());

					cc.Camera.SetPerspectiveVerticalFOV(cameraProps["PerspectiveFOV"].as<float>());
					cc.Camera.SetNearClip(cameraProps["NearClip"].as<float>());
					cc.Camera.SetFarClip(cameraProps["FarClip"].as<float>());

					cc.Camera.SetOrthographicSize(cameraProps["OrthographicWidth"].as<float>(), cameraProps["OrthographicHeight"].as<float>());

					cc.Primary = cameraComponent["Primary"].as<bool>();
					cc.FixedAspectRatio = cameraComponent["FixedAspectRatio"].as<bool>();
				}

				auto meshComponent = entity["MeshComponent"];
				if (meshComponent)
				{
					if (!meshComponent["IsPlanet"].as<bool>())
						deserializedEntity.AddComponent<MeshComponent>(CreateRef<Mesh>(meshComponent["AssetPath"].as<std::string>()));
					else
						deserializedEntity.AddComponent<MeshComponent>(CreateRef<Mesh>());
			
					auto& mc = deserializedEntity.GetComponent<MeshComponent>();

					mc.Mesh->SetIsPlanet(meshComponent["IsPlanet"].as<bool>());
					//mc.Mesh->SetMaterial(meshComponent["Material"].as<std::string>(), MaterialLibrary::Get(meshComponent["Material"].as<std::string>()));
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
					auto& pc = deserializedEntity.AddComponent<PlanetComponent>(planetComponent["Subdivisions"].as<int16_t>(), planetComponent["PatchLevels"].as<int16_t>(), planetComponent["MaxAltitude"].as<float>(), planetComponent["MinAltitude"].as<float>(), planetComponent["Radius"].as<float>(), planetComponent["GravitationalAcceleration"].as<float>(), planetComponent["AtmosphereHeight"].as<float>(), planetComponent["AtmosphereToggle"].as<bool>(), planetComponent["InScatteringPoints"].as<int>(), planetComponent["OpticalDepthPoints"].as<int>(), planetComponent["MieAnisotropy"].as<float>(), planetComponent["RayScaleHeight"].as<float>(), planetComponent["MieScaleHeight"].as<float>(), planetComponent["RayBaseScatteringCoefficient"].as<DirectX::XMFLOAT3>(), planetComponent["MieBaseScatteringCoefficient"].as<float>());
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
					auto& sc = deserializedEntity.AddComponent<ScriptComponent>();
					sc.ClassName = scriptComponent["ClassName"].as<std::string>();

					auto scriptFields = scriptComponent["ScriptFields"];
					if (scriptFields)
					{
						Ref<ScriptClass> entityClass = ScriptEngine::GetEntityClass(sc.ClassName);
						if (entityClass) 
						{
							const auto& fields = entityClass->GetFields();
							auto& entityFields = ScriptEngine::GetScriptFieldMap(deserializedEntity);

							for (auto scriptField : scriptFields)
							{
								std::string name = scriptField["Name"].as<std::string>();
								std::string typeString = scriptField["Type"].as<std::string>();
								ScriptFieldType type = Utils::ScriptFieldTypeFromString(typeString);

								ScriptFieldInstance& fieldInstance = entityFields[name];

								// Makes this into a Toaster warning
								TOAST_CORE_ASSERT(fields.find(name) != fields.end(), "");

								if (fields.find(name) == fields.end())
									continue;

								fieldInstance.Field = fields.at(name);

								switch (type)
								{
									READ_SCRIPT_FIELD(Float, float);
									READ_SCRIPT_FIELD(Double, double);
									READ_SCRIPT_FIELD(Bool, bool);
									READ_SCRIPT_FIELD(Char, char);
									READ_SCRIPT_FIELD(Byte, int8_t);
									READ_SCRIPT_FIELD(Short, int16_t);
									READ_SCRIPT_FIELD(Int, int32_t);
									READ_SCRIPT_FIELD(Long, int64_t);
									READ_SCRIPT_FIELD(UByte, uint8_t);
									READ_SCRIPT_FIELD(UShort, uint16_t);
									READ_SCRIPT_FIELD(UInt, uint32_t);
									READ_SCRIPT_FIELD(ULong, uint64_t);
									READ_SCRIPT_FIELD(Vector2, DirectX::XMFLOAT2);
									READ_SCRIPT_FIELD(Vector3, DirectX::XMFLOAT3);
									READ_SCRIPT_FIELD(Vector4, DirectX::XMFLOAT4);
									READ_SCRIPT_FIELD(Entity, UUID);
								}
							}
						}		
					}
				}

				auto rigidBodyComponent = entity["RigidBodyComponent"];
				if (rigidBodyComponent)
				{
					auto& rbc = deserializedEntity.AddComponent<RigidBodyComponent>();

					rbc.CenterOfMass = rigidBodyComponent["CenterOfMass"].as<DirectX::XMFLOAT3>();
					rbc.InvMass = rigidBodyComponent["InvMass"].as<float>();
				}

				auto sphereColliderComponent = entity["SphereColliderComponent"];
				if (sphereColliderComponent)
				{
					auto& scc = deserializedEntity.AddComponent<SphereColliderComponent>();

					scc.Radius = sphereColliderComponent["Radius"].as<float>();
					scc.RenderCollider = sphereColliderComponent["RenderCollider"].as<bool>();
				}

				auto terrainColliderComponent = entity["TerrainColliderComponent"];
				if (terrainColliderComponent)
				{
					auto& tcc = deserializedEntity.AddComponent<TerrainColliderComponent>();

					tcc.FilePath = terrainColliderComponent["AssetPath"].as<std::string>();
					tcc.TerrainData = PhysicsEngine::LoadTerrainData(tcc.FilePath.c_str());
				}

				auto uiPanelComponent = entity["UIPanelComponent"];
				if (uiPanelComponent)
				{
					auto& uipc = deserializedEntity.AddComponent<UIPanelComponent>(CreateRef<UIPanel>());
					
					uipc.Panel->SetColor(uiPanelComponent["Color"].as<DirectX::XMFLOAT4>());
					uipc.Panel->SetCornerRadius(uiPanelComponent["CornerRadius"].as<float>());
				}

				auto uiButtonComponent = entity["UIButtonComponent"];
				if (uiButtonComponent)
				{
					auto& ubc = deserializedEntity.AddComponent<UIButtonComponent>(CreateRef<UIButton>());

					ubc.Button->SetColor(uiButtonComponent["Color"].as<DirectX::XMFLOAT4>());
					ubc.Button->SetClickColor(uiButtonComponent["Color"].as<DirectX::XMFLOAT4>());
					ubc.Button->SetCornerRadius(uiButtonComponent["CornerRadius"].as<float>());
				}

				auto uiTextComponent = entity["UITextComponent"];
				if (uiTextComponent)
				{
					auto& uitc = deserializedEntity.AddComponent<UITextComponent>(CreateRef<UIText>());

					uitc.Text->SetFont(CreateRef<Font>(uiTextComponent["AssetPath"].as<std::string>()));
					uitc.Text->SetText(uiTextComponent["Text"].as<std::string>());
					uitc.Text->InvalidateText();
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