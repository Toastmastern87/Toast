#include "tpch.h"
#include "Prefab.h"

#include "Toast/Scripting/ScriptEngine.h"

#include "Toast/Scene/SceneSerializer.h"

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

}

namespace Toast {

	////////////////////////////////////////////////////////////////////////////////////////  
	//     PREFAB        ///////////////////////////////////////////////////////////////////  
	////////////////////////////////////////////////////////////////////////////////////////

	static void SerializeEntity(YAML::Emitter& out, Entity entity)
	{
		out << YAML::BeginMap; // Entity
		out << YAML::Key << "Entity" << YAML::Value << 0;

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
	}

	void Prefab::Create(Entity entity, std::string& name)
	{
		mScene = Scene::CreateEmpty();

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

	Entity Prefab::CreatePrefabFromEntity(Entity entity)
	{
		Entity newEntity = mScene->CreateEntity();

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

		for (auto childId : entity.Children())
		{
			Entity childDuplicate = CreatePrefabFromEntity(entity.mScene->FindEntityByUUID(childId));

			childDuplicate.SetParentUUID(newEntity.GetUUID());
			newEntity.Children().push_back(childDuplicate.GetUUID());
		}

		return newEntity;
	}

	////////////////////////////////////////////////////////////////////////////////////////  
	//     PREFAB LIBRARY     //////////////////////////////////////////////////////////////  
	//////////////////////////////////////////////////////////////////////////////////////// 

	std::unordered_map<std::string, Scope<Prefab>> PrefabLibrary::mPrefabs;

	Prefab* PrefabLibrary::Load(Entity entity, std::string& name)
	{
		if (Exists(name))
			return mPrefabs[name].get();

		mPrefabs[name] = CreateScope<Prefab>();
		mPrefabs[name]->Create(entity, name);
		return mPrefabs[name].get();
	}

	bool PrefabLibrary::Exists(std::string& name)
	{
		return mPrefabs.find(name) != mPrefabs.end();
	}

}