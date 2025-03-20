#include "tpch.h"
#include "Prefab.h"

#include "Toast/Scripting/ScriptEngine.h"

#include "Toast/Scene/SceneSerializer.h"

#include "Toast/Physics/PhysicsEngine.h"

#include <yaml-cpp/yaml.h>

namespace YAML {

	template<>
	struct convert<Toast::Vector3>
	{
		static Node encode(const Toast::Vector3& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, Toast::Vector3& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x = node[0].as<double>();
			rhs.y = node[1].as<double>();
			rhs.z = node[2].as<double>();
			return true;
		}
	};

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

	////////////////////////////////////////////////////////////////////////////////////////  
	//     PREFAB        ///////////////////////////////////////////////////////////////////  
	////////////////////////////////////////////////////////////////////////////////////////

	std::vector<Entity> Prefab::GetEntities() const
	{
		std::vector<Entity> entities;
		GatherEntities(mEntity, entities);
		return entities;
	}

	void Prefab::GatherEntities(Entity entity, std::vector<Entity>& outEntities) const
	{
		outEntities.push_back(entity);
		for (auto childUUID : entity.Children())
		{
			// Find the actual child entity from the scene.
			Entity child = mScene->FindEntityByUUID(childUUID);
			if (child) 
			{
				GatherEntities(child, outEntities);
			}
		}
	}

	static void DeserializeEntity(YAML::detail::iterator_value& entityData, Entity& deserializedEntity)
	{
		if (entityData["TagComponent"])
		{
			std::string tag = entityData["TagComponent"]["Tag"].as<std::string>();
			deserializedEntity.GetComponent<TagComponent>().Tag = tag;
		}

		auto transformComponent = entityData["TransformComponent"];
		if (transformComponent)
		{
			// Entities always have transforms
			auto& tc = deserializedEntity.GetComponent<TransformComponent>();
			tc.Translation = transformComponent["Translation"].as<DirectX::XMFLOAT3>();
			tc.RotationEulerAngles = transformComponent["Rotation"].as<DirectX::XMFLOAT3>();
			tc.Scale = transformComponent["Scale"].as<DirectX::XMFLOAT3>();
		}

		auto cameraComponent = entityData["CameraComponent"];
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

		auto meshComponent = entityData["MeshComponent"];
		if (meshComponent)
		{
			std::string assetPath = meshComponent["AssetPath"].as<std::string>();

			deserializedEntity.AddComponent<MeshComponent>(CreateRef<Mesh>(assetPath));

			auto& mc = deserializedEntity.GetComponent<MeshComponent>();

			if (meshComponent["LODThresholds"])
			{
				const YAML::Node& thresholdsNode = meshComponent["LODThresholds"];
				std::vector<float>& thresholds = mc.MeshObject->GetLODThresholds();
				thresholds.clear(); // Ensure the vector is empty before adding
				for (std::size_t i = 0; i < thresholdsNode.size(); ++i)
				{
					thresholds.emplace_back(thresholdsNode[i].as<float>());
				}
			}
		}

		auto spriteRendererComponent = entityData["SpriteRendererComponent"];
		if (spriteRendererComponent)
		{
			auto& src = deserializedEntity.AddComponent<SpriteRendererComponent>();
			src.Color = spriteRendererComponent["Color"].as<DirectX::XMFLOAT4>();
		}

		auto planetComponent = entityData["PlanetComponent"];
		if (planetComponent)
		{
			auto& pc = deserializedEntity.AddComponent<PlanetComponent>(planetComponent["Subdivisions"].as<int16_t>(), planetComponent["MaxAltitude"].as<float>(), planetComponent["MinAltitude"].as<float>(), planetComponent["Radius"].as<float>(), planetComponent["GravitationalAcceleration"].as<float>(), planetComponent["SmoothShading"].as<bool>(), planetComponent["AtmosphereHeight"].as<float>(), planetComponent["AtmosphereToggle"].as<bool>(), planetComponent["InScatteringPoints"].as<int>(), planetComponent["OpticalDepthPoints"].as<int>(), planetComponent["MieAnisotropy"].as<float>(), planetComponent["RayScaleHeight"].as<float>(), planetComponent["MieScaleHeight"].as<float>(), planetComponent["RayBaseScatteringCoefficient"].as<DirectX::XMFLOAT3>(), planetComponent["MieBaseScatteringCoefficient"].as<float>(), planetComponent["SunDisc"].as<bool>(), planetComponent["SunDiscRadius"].as<float>(), planetComponent["SunGlowIntensity"].as<float>(), planetComponent["SunEdgeSoftness"].as<float>(), planetComponent["SunGlowSize"].as<float>());
		}

		auto skylightComponent = entityData["SkyLightComponent"];
		if (skylightComponent)
		{
			auto& skc = deserializedEntity.AddComponent<SkyLightComponent>();

			skc.SceneEnvironment = Environment::Load(skylightComponent["AssetPath"].as<std::string>());
			skc.Intensity = skylightComponent["Intensity"].as<float>();
		}

		auto directionalLightComponent = entityData["DirectionalLightComponent"];
		if (directionalLightComponent)
		{
			auto& dlc = deserializedEntity.AddComponent<DirectionalLightComponent>();

			dlc.Radiance = directionalLightComponent["Radiance"].as<DirectX::XMFLOAT3>();
			dlc.Intensity = directionalLightComponent["Intensity"].as<float>();
			dlc.SunDesiredCoverage = directionalLightComponent["SunDesiredCoverage"].as<float>();
			dlc.SunLightDistance = directionalLightComponent["SunLightDistance"].as<float>();
		}

		auto scriptComponent = entityData["ScriptComponent"];
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

		auto rigidBodyComponent = entityData["RigidBodyComponent"];
		if (rigidBodyComponent)
		{
			auto& rbc = deserializedEntity.AddComponent<RigidBodyComponent>();

			rbc.CenterOfMass = rigidBodyComponent["CenterOfMass"].as<Vector3>();
			rbc.InvMass = rigidBodyComponent["InvMass"].as<double>();
			rbc.Elasticity = rigidBodyComponent["Elasticity"].as<double>();
			rbc.Friction = rigidBodyComponent["Friction"].as<double>();
			rbc.LinearDamping = rigidBodyComponent["LinearDamping"].as<double>();
			rbc.AngularDamping = rigidBodyComponent["AngularDamping"].as<double>();
		}

		auto sphereColliderComponent = entityData["SphereColliderComponent"];
		if (sphereColliderComponent)
		{
			auto& scc = deserializedEntity.AddComponent<SphereColliderComponent>();

			scc.Collider->mRadius = sphereColliderComponent["Radius"].as<float>();
			scc.RenderCollider = sphereColliderComponent["RenderCollider"].as<bool>();

			scc.Collider->CalculateBounds();
		}

		auto boxColliderComponent = entityData["BoxColliderComponent"];
		if (boxColliderComponent)
		{
			auto& bcc = deserializedEntity.AddComponent<BoxColliderComponent>();

			bcc.Collider->mSize = boxColliderComponent["Size"].as<DirectX::XMFLOAT3>();
			bcc.RenderCollider = boxColliderComponent["RenderCollider"].as<bool>();

			bcc.Collider->CalculateBounds();
		}

		auto terrainColliderComponent = entityData["TerrainColliderComponent"];
		if (terrainColliderComponent)
		{
			auto& tcc = deserializedEntity.AddComponent<TerrainColliderComponent>();

			if (planetComponent)
			{
				PlanetComponent& pc = deserializedEntity.GetComponent<PlanetComponent>();

				tcc.Collider->mFilePath = terrainColliderComponent["AssetPath"].as<std::string>();
				if (!tcc.Collider->mFilePath.empty())
					pc.TerrainData = PhysicsEngine::LoadTerrainData(tcc.Collider->mFilePath.c_str(), pc.PlanetData.maxAltitude, pc.PlanetData.minAltitude);

				PlanetSystem::CalculateBasePlanet(pc, pc.PlanetData.radius);

				tcc.Collider->mMaxAltitude = planetComponent["MaxAltitude"].as<float>() + planetComponent["Radius"].as<float>();
				tcc.Collider->CalculateBounds();
			}
		}

		auto uiPanelComponent = entityData["UIPanelComponent"];
		if (uiPanelComponent)
		{
			auto& tc = deserializedEntity.GetComponent<TransformComponent>();
			auto& uipc = deserializedEntity.AddComponent<UIPanelComponent>(CreateRef<UIPanel>(tc.Translation.x, tc.Translation.y, tc.Scale.x, tc.Scale.y));

			uipc.Panel->SetColor(uiPanelComponent["Color"].as<DirectX::XMFLOAT4>());
			uipc.Panel->SetCornerRadius(uiPanelComponent["CornerRadius"].as<float>());
			uipc.Panel->SetBorderSize(uiPanelComponent["BorderSize"].as<float>());
			uipc.Panel->SetUseColor(uiPanelComponent["UseColor"].as<bool>());
			uipc.Panel->SetVisible(uiPanelComponent["Visible"].as<bool>());
			uipc.Panel->SetConnectToParent(uiPanelComponent["ConnectToParent"].as<bool>());

			uipc.Panel->SetTextureFilepath(uiPanelComponent["AssetPath"].as<std::string>());
			if (!uipc.Panel->GetTextureFilepath().empty())
				TextureLibrary::LoadTexture2D(uipc.Panel->GetTextureFilepath());
		}

		auto uiButtonComponent = entityData["UIButtonComponent"];
		if (uiButtonComponent)
		{
			auto& ubc = deserializedEntity.AddComponent<UIButtonComponent>(CreateRef<UIButton>());

			ubc.Button->SetColor(uiButtonComponent["Color"].as<DirectX::XMFLOAT4>());
			ubc.Button->SetClickColor(uiButtonComponent["Color"].as<DirectX::XMFLOAT4>());
			ubc.Button->SetCornerRadius(uiButtonComponent["CornerRadius"].as<float>());
		}

		auto uiTextComponent = entityData["UITextComponent"];
		if (uiTextComponent)
		{
			auto& uitc = deserializedEntity.AddComponent<UITextComponent>(CreateRef<UIText>());

			uitc.Text->SetFont(CreateRef<Font>(uiTextComponent["AssetPath"].as<std::string>()));
			uitc.Text->SetText(uiTextComponent["Text"].as<std::string>());
		}

		auto terrainDetailComponent = entityData["TerrainDetailComponent"];
		if (terrainDetailComponent)
		{
			auto& tdc = deserializedEntity.AddComponent<TerrainDetailComponent>();

			if (terrainDetailComponent["Seed"].as<uint32_t>() != 0)
				tdc.Seed = terrainDetailComponent["Seed"].as<uint32_t>();
			tdc.SubdivisionActivation = terrainDetailComponent["SubdivisionActivation"].as<int>();
			tdc.Octaves = terrainDetailComponent["Octaves"].as<int>();
			tdc.Frequency = terrainDetailComponent["Frequency"].as<float>();
			tdc.Amplitude = terrainDetailComponent["Amplitude"].as<float>();
		}

		auto terrainObjectComponent = entityData["TerrainObjectComponent"];
		if (terrainObjectComponent)
		{
			auto& toc = deserializedEntity.AddComponent<TerrainObjectComponent>();

			toc.MaxNrOfObjects = terrainObjectComponent["MaxNumberOfObjects"].as<int>();

			toc.MeshObject = CreateRef<Mesh>(terrainObjectComponent["AssetPath"].as<std::string>(), DirectX::XMFLOAT3(0.0, 0.0, 0.0), true, toc.MaxNrOfObjects);

			toc.SubdivisionActivation = terrainObjectComponent["SubdivisionActivation"].as<int>();
			toc.MaxNrOfObjectPerFace = terrainObjectComponent["MaxNumberOfObjectsPerFace"].as<int>();
		}

		auto particlesComponent = entityData["ParticlesComponent"];
		if (particlesComponent)
		{
			auto& pc = deserializedEntity.AddComponent<ParticlesComponent>();

			pc.Emitting = particlesComponent["Emitting"].as<bool>();

			pc.MaxLifeTime = particlesComponent["MaxLifeTime"].as<float>();
			pc.SpawnDelay = particlesComponent["SpawnDelay"].as<float>();
			pc.Velocity = particlesComponent["Velocity"].as<DirectX::XMFLOAT3>();
			pc.StartColor = particlesComponent["StartColor"].as<DirectX::XMFLOAT3>();
			pc.EndColor = particlesComponent["EndColor"].as<DirectX::XMFLOAT3>();
			pc.ColorBlendFactor = particlesComponent["ColorBlendFactor"].as<float>();
			pc.BiasExponent = particlesComponent["BiasExponent"].as<float>();
			pc.ConeAngleDegrees = particlesComponent["ConeAngleDegrees"].as<float>();
			pc.SpawnFunction = static_cast<EmitFunction>(particlesComponent["SpawnFunction"].as<uint16_t>());
			pc.GrowRate = particlesComponent["GrowRate"].as<float>();
			pc.BurstInitial = particlesComponent["BurstInitial"].as<float>();
			pc.BurstDecay = particlesComponent["BurstDecay"].as<float>();
			pc.Size = particlesComponent["Size"].as<float>();

			pc.MaskTexture = TextureLibrary::LoadTexture2D(particlesComponent["AssetPath"].as<std::string>());
		}
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

		if (entity.HasComponent<PrefabComponent>())
		{
			out << YAML::Key << "PrefabComponent";
			out << YAML::BeginMap; // PrefabComponent

			auto& pc = entity.GetComponent<PrefabComponent>();
			out << YAML::Key << "PrefabHandle" << YAML::Value << pc.PrefabHandle;

			out << YAML::EndMap; // PrefabComponent
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
			auto& cc = entity.GetComponent<CameraComponent>();
			auto& camera = cc.Camera;

			if (!cc.Primary)
			{
				out << YAML::Key << "CameraComponent";
				out << YAML::BeginMap; // CameraComponent

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
		}

		if (entity.HasComponent<MeshComponent>())
		{
			out << YAML::Key << "MeshComponent";
			out << YAML::BeginMap; // MeshComponent

			auto& mc = entity.GetComponent<MeshComponent>();
			out << YAML::Key << "AssetPath" << YAML::Value << mc.MeshObject->GetFilePath();

			out << YAML::Key << "LODThresholds" << YAML::BeginSeq;
			for (const float& threshold : mc.MeshObject->GetLODThresholds())
			{
				out << threshold;
			}
			out << YAML::EndSeq;

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
			out << YAML::Key << "MaxAltitude" << YAML::Value << pc.PlanetData.maxAltitude;
			out << YAML::Key << "MinAltitude" << YAML::Value << pc.PlanetData.minAltitude;
			out << YAML::Key << "Radius" << YAML::Value << pc.PlanetData.radius;
			out << YAML::Key << "GravitationalAcceleration" << YAML::Value << pc.PlanetData.gravAcc;
			out << YAML::Key << "SmoothShading" << YAML::Value << pc.PlanetData.smoothShading;
			out << YAML::Key << "AtmosphereHeight" << YAML::Value << pc.PlanetData.atmosphereHeight;
			out << YAML::Key << "AtmosphereToggle" << YAML::Value << pc.PlanetData.atmosphereToggle;
			out << YAML::Key << "InScatteringPoints" << YAML::Value << pc.PlanetData.inScatteringPoints;
			out << YAML::Key << "OpticalDepthPoints" << YAML::Value << pc.PlanetData.opticalDepthPoints;
			out << YAML::Key << "MieAnisotropy" << YAML::Value << pc.PlanetData.mieAnisotropy;
			out << YAML::Key << "RayScaleHeight" << YAML::Value << pc.PlanetData.rayScaleHeight;
			out << YAML::Key << "MieScaleHeight" << YAML::Value << pc.PlanetData.mieScaleHeight;
			out << YAML::Key << "RayBaseScatteringCoefficient" << YAML::Value << pc.PlanetData.rayBaseScatteringCoefficient;
			out << YAML::Key << "MieBaseScatteringCoefficient" << YAML::Value << pc.PlanetData.mieBaseScatteringCoefficient;
			out << YAML::Key << "SunDisc" << YAML::Value << pc.PlanetData.SunDisc;
			out << YAML::Key << "SunDiscRadius" << YAML::Value << pc.PlanetData.SunDiscRadius;
			out << YAML::Key << "SunGlowIntensity" << YAML::Value << pc.PlanetData.SunGlowIntensity;
			out << YAML::Key << "SunEdgeSoftness" << YAML::Value << pc.PlanetData.SunEdgeSoftness;
			out << YAML::Key << "SunGlowSize" << YAML::Value << pc.PlanetData.SunGlowSize;

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
			out << YAML::Key << "SunDesiredCoverage" << YAML::Value << dlc.SunDesiredCoverage;
			out << YAML::Key << "SunLightDistance" << YAML::Value << dlc.SunLightDistance;

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
			if (fields.size() > 0)
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
						WRITE_SCRIPT_FIELD(Float, float);
						WRITE_SCRIPT_FIELD(Double, double);
						WRITE_SCRIPT_FIELD(Bool, bool);
						WRITE_SCRIPT_FIELD(Char, char);
						WRITE_SCRIPT_FIELD(Byte, int8_t);
						WRITE_SCRIPT_FIELD(Short, int16_t);
						WRITE_SCRIPT_FIELD(Int, int32_t);
						WRITE_SCRIPT_FIELD(Long, int64_t);
						WRITE_SCRIPT_FIELD(UByte, uint8_t);
						WRITE_SCRIPT_FIELD(UShort, uint16_t);
						WRITE_SCRIPT_FIELD(UInt, uint32_t);
						WRITE_SCRIPT_FIELD(ULong, uint64_t);
						WRITE_SCRIPT_FIELD(Vector2, DirectX::XMFLOAT2);
						WRITE_SCRIPT_FIELD(Vector3, DirectX::XMFLOAT3);
						WRITE_SCRIPT_FIELD(Vector4, DirectX::XMFLOAT4);
						WRITE_SCRIPT_FIELD(Entity, UUID);
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
			out << YAML::Key << "Elasticity" << YAML::Value << rbc.Elasticity;
			out << YAML::Key << "Friction" << YAML::Value << rbc.Friction;
			out << YAML::Key << "CenterOfMass" << YAML::Value << rbc.CenterOfMass;
			out << YAML::Key << "LinearDamping" << YAML::Value << rbc.LinearDamping;
			out << YAML::Key << "AngularDamping" << YAML::Value << rbc.AngularDamping;

			out << YAML::EndMap; // RigidBodyComponent
		}

		if (entity.HasComponent<SphereColliderComponent>())
		{
			out << YAML::Key << "SphereColliderComponent";
			out << YAML::BeginMap; // SphereColliderComponent

			auto& scc = entity.GetComponent<SphereColliderComponent>();
			out << YAML::Key << "RenderCollider" << YAML::Value << scc.RenderCollider;
			out << YAML::Key << "Radius" << YAML::Value << scc.Collider->mRadius;

			out << YAML::EndMap; // SphereColliderComponent
		}

		if (entity.HasComponent<BoxColliderComponent>())
		{
			out << YAML::Key << "BoxColliderComponent";
			out << YAML::BeginMap; // BoxColliderComponent

			auto& bcc = entity.GetComponent<BoxColliderComponent>();
			out << YAML::Key << "RenderCollider" << YAML::Value << bcc.RenderCollider;
			out << YAML::Key << "Size" << YAML::Value << bcc.Collider->mSize;

			out << YAML::EndMap; // BoxColliderComponent
		}

		if (entity.HasComponent<TerrainColliderComponent>())
		{
			out << YAML::Key << "TerrainColliderComponent";
			out << YAML::BeginMap; // TerrainColliderComponent

			auto& tcc = entity.GetComponent<TerrainColliderComponent>();
			out << YAML::Key << "AssetPath" << YAML::Value << tcc.Collider->mFilePath;

			out << YAML::EndMap; // TerrainColliderComponent
		}

		if (entity.HasComponent<UIPanelComponent>())
		{
			out << YAML::Key << "UIPanelComponent";
			out << YAML::BeginMap; // UIPanelComponent

			auto& uipc = entity.GetComponent<UIPanelComponent>();
			out << YAML::Key << "Color" << YAML::Value << uipc.Panel->GetColorF4();
			out << YAML::Key << "CornerRadius" << YAML::Value << *uipc.Panel->GetCornerRadius();
			out << YAML::Key << "BorderSize" << YAML::Value << *uipc.Panel->GetBorderSize();
			out << YAML::Key << "AssetPath" << YAML::Value << uipc.Panel->GetTextureFilepath();
			out << YAML::Key << "UseColor" << YAML::Value << uipc.Panel->GetUseColor();
			out << YAML::Key << "Visible" << YAML::Value << uipc.Panel->GetVisible();
			out << YAML::Key << "ConnectToParent" << YAML::Value << uipc.Panel->GetConnectToParent();

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

		if (entity.HasComponent<TerrainDetailComponent>())
		{
			out << YAML::Key << "TerrainDetailComponent";
			out << YAML::BeginMap; // TerrainDetailComponent

			auto& tdc = entity.GetComponent<TerrainDetailComponent>();
			out << YAML::Key << "Seed" << YAML::Value << tdc.Seed;
			out << YAML::Key << "SubdivisionActivation" << YAML::Value << tdc.SubdivisionActivation;
			out << YAML::Key << "Octaves" << YAML::Value << tdc.Octaves;
			out << YAML::Key << "Frequency" << YAML::Value << tdc.Frequency;
			out << YAML::Key << "Amplitude" << YAML::Value << tdc.Amplitude;

			out << YAML::EndMap; // TerrainDetailComponent
		}

		if (entity.HasComponent<TerrainObjectComponent>())
		{
			out << YAML::Key << "TerrainObjectComponent";
			out << YAML::BeginMap; // TerrainObjectComponent

			auto& toc = entity.GetComponent<TerrainObjectComponent>();
			if (toc.MeshObject)
				out << YAML::Key << "AssetPath" << YAML::Value << toc.MeshObject->GetFilePath();
			out << YAML::Key << "SubdivisionActivation" << YAML::Value << toc.SubdivisionActivation;
			out << YAML::Key << "MaxNumberOfObjectsPerFace" << YAML::Value << toc.MaxNrOfObjectPerFace;
			out << YAML::Key << "MaxNumberOfObjects" << YAML::Value << toc.MaxNrOfObjects;
			out << YAML::EndMap; // TerrainObjectComponent
		}

		if (entity.HasComponent<ParticlesComponent>())
		{
			out << YAML::Key << "ParticlesComponent";
			out << YAML::BeginMap; // ParticlesComponent

			auto& pc = entity.GetComponent<ParticlesComponent>();
			out << YAML::Key << "Emitting" << YAML::Value << pc.Emitting;
			out << YAML::Key << "MaxLifeTime" << YAML::Value << pc.MaxLifeTime;
			out << YAML::Key << "SpawnDelay" << YAML::Value << pc.SpawnDelay;
			out << YAML::Key << "Velocity" << YAML::Value << pc.Velocity;
			out << YAML::Key << "StartColor" << YAML::Value << pc.StartColor;
			out << YAML::Key << "EndColor" << YAML::Value << pc.EndColor;
			out << YAML::Key << "ColorBlendFactor" << YAML::Value << pc.ColorBlendFactor;
			out << YAML::Key << "ConeAngleDegrees" << YAML::Value << pc.ConeAngleDegrees;
			out << YAML::Key << "BiasExponent" << YAML::Value << pc.BiasExponent;
			out << YAML::Key << "GrowRate" << YAML::Value << pc.GrowRate;
			out << YAML::Key << "BurstInitial" << YAML::Value << pc.BurstInitial;
			out << YAML::Key << "BurstDecay" << YAML::Value << pc.BurstDecay;
			out << YAML::Key << "Size" << YAML::Value << pc.Size;
			out << YAML::Key << "SpawnFunction" << YAML::Value << static_cast<uint16_t>(pc.SpawnFunction);
			out << YAML::Key << "AssetPath" << YAML::Value << pc.MaskTexture->GetFilePath();
			out << YAML::EndMap; // ParticlesComponent
		}

		out << YAML::EndMap; // Entity
	}

	template<typename T>
	static void CopyComponentIfExists(entt::entity dst, entt::registry& dstRegistry, entt::entity src, entt::registry& srcRegistry)
	{
		if (srcRegistry.has<T>(src))
		{
			auto& srcComponent = srcRegistry.get<T>(src);
			dstRegistry.emplace_or_replace<T>(dst, srcComponent);
		}
	}

	Prefab::Prefab()
	{
		mScene = Scene::CreateEmpty();
	}

	void Prefab::Create(Entity entity, std::string& name)
	{
		mEntity = CreatePrefabFromEntity(entity);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Prefab Name" << YAML::Value << name;
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

		// TO DO make sure it is serialized to the correct folder and with correct name
		std::string filepath = "C:\\dev\\Toast\\Toaster\\assets\\prefabs\\" + name + ".ptoast";
		std::ofstream fout(filepath);
		fout << out.c_str();

		TOAST_CORE_INFO("Prefab created with name: %s", name.c_str());
	}

	Entity Prefab::LoadFromFile(std::string& name)
	{
		// Clear the prefab scene registry so we load fresh data.
		mScene->mRegistry.clear();
		mScene->mEntityIDMap.clear();

		// Construct the absolute file path (adjust as needed for your project)
		std::string filepath = "C:\\dev\\Toast\\Toaster\\assets\\prefabs\\" + name + ".ptoast";

		YAML::Node data;
		try {
			data = YAML::LoadFile(filepath);
		}
		catch (const YAML::ParserException& ex)
		{
			TOAST_CORE_ERROR("Failed to load prefab file %s\n%s", filepath.c_str(), ex.what());
			mEntity = mScene->CreateEntity();
			return mEntity;
		}

		YAML::Node entitiesNode = data["Entities"];
		// Mapping from file's UUID to the new entity (using file IDs as temporary keys)
		std::unordered_map<UUID, Entity> fileIDToEntity;

		if (entitiesNode)
		{
			// First pass: create new entities with new UUIDs, ignoring the file UUID.
			for (auto entityNode : entitiesNode)
			{
				// Read the file's UUID (used only for mapping).
				uint64_t fileUUID = entityNode["Entity"].as<UUID>();
				std::string tagName = "Entity";
				if (entityNode["TagComponent"])
					tagName = entityNode["TagComponent"]["Tag"].as<std::string>();

				// Create a new entity with a fresh UUID.
				Entity newEntity = mScene->CreateEntity(tagName);
				newEntity.Children().clear();
				fileIDToEntity[fileUUID] = newEntity;

				// Deserialize additional components.
				DeserializeEntity(entityNode, newEntity);
			}
		}
		// Second pass: update parent–child relationships based on the file data.
		if (entitiesNode)
		{
			for (auto entityNode : entitiesNode)
			{
				uint64_t fileUUID = entityNode["Entity"].as<UUID>();
				if (entityNode["RelationshipComponent"])
				{
					YAML::Node relNode = entityNode["RelationshipComponent"];
					if (relNode["Children"])
					{
						for (auto childNode : relNode["Children"])
						{
							uint64_t childFileUUID = childNode["Handle"].as<UUID>();
							if (fileIDToEntity.find(fileUUID) != fileIDToEntity.end() &&
								fileIDToEntity.find(childFileUUID) != fileIDToEntity.end())
							{
								Entity parentEntity = fileIDToEntity[fileUUID];
								Entity childEntity = fileIDToEntity[childFileUUID];
								childEntity.SetParentUUID(parentEntity.GetUUID());
								parentEntity.Children().push_back(childEntity.GetUUID());
							}
						}
					}
				}
			}
		}

		UUID rootFileUUID = 0;
		if (entitiesNode && entitiesNode.size() > 0)
		{
			for (auto entityNode : entitiesNode)
			{
				// If there's a RelationshipComponent and ParentHandle is 0, this is the root.
				if (entityNode["RelationshipComponent"])
				{
					uint64_t parentHandle = entityNode["RelationshipComponent"]["ParentHandle"].as<uint64_t>();
					if (parentHandle == 0)
					{
						rootFileUUID = entityNode["Entity"].as<UUID>();
						break;
					}
				}
			}
			// If none found, fall back to the first entity.
			if (rootFileUUID == 0)
				rootFileUUID = entitiesNode[0]["Entity"].as<UUID>();

			mEntity = fileIDToEntity[rootFileUUID];
		}
		else
		{
			mEntity = mScene->CreateEntity();
		}

		return mEntity;
	}

	void Prefab::Update(Entity entity, std::string& name)
	{
		mScene->mRegistry.clear();

		mEntity = CreatePrefabFromEntity(entity);

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Prefab Name" << YAML::Value << name;
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

		// TO DO make sure it is serialized to the correct folder and with correct name
		std::string filepath = "C:\\dev\\Toast\\Toaster\\assets\\prefabs\\" + name + ".ptoast";
		std::ofstream fout(filepath);
		fout << out.c_str();

		TOAST_CORE_INFO("Prefab with name: %s updated", name.c_str());
	}

	Entity Prefab::CreatePrefabFromEntity(Entity entity)
	{
		std::string name = "Entity";

		if (entity.HasComponent<RelationshipComponent>())
		{
			auto& rc = entity.GetComponent<RelationshipComponent>();

			if(rc.ParentHandle > 0)
				name = entity.GetComponent<TagComponent>().Tag;
		}

		Entity newEntity = mScene->CreateEntity(name);

		CopyComponentIfExists<RelationshipComponent>(newEntity, mScene->mRegistry, entity, entity.mScene->mRegistry);
		CopyComponentIfExists<PrefabComponent>(newEntity, mScene->mRegistry, entity, entity.mScene->mRegistry);
		CopyComponentIfExists<TransformComponent>(newEntity, mScene->mRegistry, entity, entity.mScene->mRegistry);
		CopyComponentIfExists<MeshComponent>(newEntity, mScene->mRegistry, entity, entity.mScene->mRegistry);
		CopyComponentIfExists<PlanetComponent>(newEntity, mScene->mRegistry, entity, entity.mScene->mRegistry);
		CopyComponentIfExists<CameraComponent>(newEntity, mScene->mRegistry, entity, entity.mScene->mRegistry);
		CopyComponentIfExists<SpriteRendererComponent>(newEntity, mScene->mRegistry, entity, entity.mScene->mRegistry);
		CopyComponentIfExists<DirectionalLightComponent>(newEntity, mScene->mRegistry, entity, entity.mScene->mRegistry);
		CopyComponentIfExists<SkyLightComponent>(newEntity, mScene->mRegistry, entity, entity.mScene->mRegistry);
		CopyComponentIfExists<ScriptComponent>(newEntity, mScene->mRegistry, entity, entity.mScene->mRegistry);
		CopyComponentIfExists<RigidBodyComponent>(newEntity, mScene->mRegistry, entity, entity.mScene->mRegistry);
		CopyComponentIfExists<SphereColliderComponent>(newEntity, mScene->mRegistry, entity, entity.mScene->mRegistry);
		CopyComponentIfExists<BoxColliderComponent>(newEntity, mScene->mRegistry, entity, entity.mScene->mRegistry);
		CopyComponentIfExists<TerrainColliderComponent>(newEntity, mScene->mRegistry, entity, entity.mScene->mRegistry);
		CopyComponentIfExists<UIPanelComponent>(newEntity, mScene->mRegistry, entity, entity.mScene->mRegistry);
		CopyComponentIfExists<UITextComponent>(newEntity, mScene->mRegistry, entity, entity.mScene->mRegistry);
		CopyComponentIfExists<UIButtonComponent>(newEntity, mScene->mRegistry, entity, entity.mScene->mRegistry);
		CopyComponentIfExists<TerrainDetailComponent>(newEntity, mScene->mRegistry, entity, entity.mScene->mRegistry);
		CopyComponentIfExists<TerrainObjectComponent>(newEntity, mScene->mRegistry, entity, entity.mScene->mRegistry);
		CopyComponentIfExists<ParticlesComponent>(newEntity, mScene->mRegistry, entity, entity.mScene->mRegistry);

		// Make a local copy of the original children from the source entity.
		auto originalChildren = entity.Children();

		// Clear the new entity's children list to avoid reusing stale or copied children.
		if (newEntity.HasComponent<RelationshipComponent>())
		{
			newEntity.GetComponent<RelationshipComponent>().Children.clear();
		}

		// Recursively duplicate each child.
		for (auto childId : originalChildren)
		{
			Entity childEntity = entity.mScene->FindEntityByUUID(childId);
			if (!childEntity)
				continue;

			// Recursively create a duplicate for the child.
			Entity childDuplicate = CreatePrefabFromEntity(childEntity);
			childDuplicate.SetParentUUID(newEntity.GetUUID());
			newEntity.Children().push_back(childDuplicate.GetUUID());
		}

		return newEntity;
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//     PREFAB LIBRARY     //////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	std::unordered_map<std::string, Scope<Prefab>> PrefabLibrary::mPrefabs;

	std::vector<Entity> PrefabLibrary::GetEntities(std::string& name)
	{
		if (Exists(name))
			return mPrefabs[name]->GetEntities();
		else
		{
			mPrefabs[name] = CreateScope<Prefab>();
			mPrefabs[name]->LoadFromFile(name);
			return mPrefabs[name]->GetEntities();
		}
	}

	std::vector<Entity> PrefabLibrary::Load(Entity entity, std::string& name)
	{
		if (Exists(name))
			return mPrefabs[name]->GetEntities();

		mPrefabs[name] = CreateScope<Prefab>();
		mPrefabs[name]->Create(entity, name);
		return mPrefabs[name]->GetEntities();
	}

	Prefab* PrefabLibrary::Update(Entity entity, std::string& name)
	{
		mPrefabs[name]->Update(entity, name);

		return mPrefabs[name].get();
	}

	bool PrefabLibrary::Exists(std::string& name)
	{
		return mPrefabs.find(name) != mPrefabs.end();
	}

}