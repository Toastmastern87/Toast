#include "tpch.h"
#include "SceneSerializer.h"

#include "Entity.h"
#include "Components.h"

#include "Toast/Scene/Prefab.h"

#include "Toast/Core/Math/Vector.h"

#include "Toast/Scripting/ScriptEngine.h"

#include "Toast/Physics/PhysicsEngine.h"

namespace YAML 
{
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
	struct convert<DirectX::XMFLOAT3X3>
	{
		static Node encode(const DirectX::XMFLOAT3X3& matrix)
		{
			Node node;
			node.push_back(matrix.m[0][0]);
			node.push_back(matrix.m[0][1]);
			node.push_back(matrix.m[0][2]);
			node.push_back(matrix.m[1][0]);
			node.push_back(matrix.m[1][1]);
			node.push_back(matrix.m[1][2]);
			node.push_back(matrix.m[2][0]);
			node.push_back(matrix.m[2][1]);
			node.push_back(matrix.m[2][2]);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, DirectX::XMFLOAT3X3& matrix)
		{
			if (!node.IsSequence() || node.size() != 9)
				return false;

			matrix.m[0][0] = node[0].as<float>();
			matrix.m[0][1] = node[1].as<float>();
			matrix.m[0][2] = node[2].as<float>();
			matrix.m[1][0] = node[3].as<float>();
			matrix.m[1][1] = node[4].as<float>();
			matrix.m[1][2] = node[5].as<float>();
			matrix.m[2][0] = node[6].as<float>();
			matrix.m[2][1] = node[7].as<float>();
			matrix.m[2][2] = node[8].as<float>();

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

	SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
		: mScene(scene)
	{

	}

	static void SerializeEntity(YAML::Emitter& out, Entity entity, bool mainCamera = false)
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

		if (entity.HasComponent<PrefabComponent>())
		{
			out << YAML::Key << "PrefabComponent";
			out << YAML::BeginMap; // PrefabComponent

			auto& pc = entity.GetComponent<PrefabComponent>();
			out << YAML::Key << "PrefabHandle" << YAML::Value << pc.PrefabHandle;

			out << YAML::EndMap; // PrefabComponent

			out << YAML::EndMap; // Entity

			return;
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

			if (rc.ParentHandle > 0)
			{
				if (entity.GetScene()->FindEntityByUUID(rc.ParentHandle).HasComponent<PrefabComponent>())
				{
					out << YAML::EndMap; // Entity

					return;
				}
			}
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

			if (!cc.Primary || mainCamera) 
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
			if(toc.MeshObject)
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

	void SceneSerializer::CopyComponents(Entity& target, Entity& source)
	{
		CopyComponentIfExists<PrefabComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<TransformComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<MeshComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<PlanetComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<CameraComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<SpriteRendererComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<DirectionalLightComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<SkyLightComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<ScriptComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<RigidBodyComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<SphereColliderComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<BoxColliderComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<TerrainColliderComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<UIPanelComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<UITextComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<UIButtonComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<TerrainDetailComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<TerrainObjectComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<ParticlesComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
	}

	void SceneSerializer::InstantiatePrefabChildren(Scene* currentScene, Entity& sceneParent, Entity prefabParent)
	{
		// Check if the prefab parent has any children.
		if (!prefabParent.HasComponent<RelationshipComponent>())
			return;

		auto& prefabRel = prefabParent.GetComponent<RelationshipComponent>();

		// Loop over each child handle from the prefab parent's RelationshipComponent.
		for (auto childFileHandle : prefabRel.Children)
		{
			// In the prefab scene, find the child entity by its file handle.
			Entity prefabChild = prefabParent.GetScene()->FindEntityByUUID(childFileHandle);
			if (!prefabChild)
				continue;

			// Create a new entity in the current scene for this prefab child.
			std::string childName = "Prefab Child";
			if (prefabChild.HasComponent<TagComponent>()) 
				childName = prefabChild.GetComponent<TagComponent>().Tag;
			Entity newChild = currentScene->CreateEntity(childName);

			// Set up the parent-child relationship: newChild becomes a child of sceneParent.
			newChild.SetParentUUID(sceneParent.GetUUID());
			sceneParent.Children().push_back(newChild.GetUUID());

			// Copy all desired components from the prefab child to the new scene entity.
			CopyComponents(newChild, prefabChild);

			// Recursively instantiate any children of this prefab child.
			InstantiatePrefabChildren(currentScene, newChild, prefabChild);
		}
	}

	void SceneSerializer::Serialize(const std::string& filepath)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		// TODO, should be scene name instead of just untitled scene
		out << YAML::Key << "Scene" << YAML::Value << "Untitled Scene";
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

		// Making sure the main camera is serialized first
		mScene->mRegistry.each([&](auto entityID)
		{
			Entity entity = { entityID, mScene.get() };

			// Skip if this entity is a child of a prefab.
			if (entity.HasComponent<RelationshipComponent>())
			{
				auto parentHandle = entity.GetComponent<RelationshipComponent>().ParentHandle;
				if (parentHandle != 0)
				{
					Entity parent = mScene->FindEntityByUUID(parentHandle);
					if (parent && parent.HasComponent<PrefabComponent>())
						return; // Skip serializing this entity.
				}
			}

			if (entity.HasComponent<CameraComponent>())
			{
				if (entity.GetComponent<CameraComponent>().Primary)
					SerializeEntity(out, entity, true);
			}
		});

		mScene->mRegistry.each([&](auto entityID)
		{
			Entity entity = { entityID, mScene.get() };

			// Skip if this entity is a child of a prefab.
			if (entity.HasComponent<RelationshipComponent>())
			{
				auto parentHandle = entity.GetComponent<RelationshipComponent>().ParentHandle;
				while (parentHandle != 0)
				{
					Entity parent = mScene->FindEntityByUUID(parentHandle);
					if (!parent)
						break; // Just in case the parent doesn't exist.

					if (parent.HasComponent<PrefabComponent>())
						return; // Skip serializing this entity if any ancestor is a prefab.

					// Update parentHandle to check the next level up.
					if (parent.HasComponent<RelationshipComponent>())
						parentHandle = parent.GetComponent<RelationshipComponent>().ParentHandle;
					else
						break; // Reached the root.
				}
			}

			if (!entity || !entity.HasComponent<IDComponent>())
				return;

			if (entity.HasComponent<CameraComponent>())
			{
				if (!entity.GetComponent<CameraComponent>().Primary)
					SerializeEntity(out, entity);					
			}
			else
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

				auto prefabComponent = entity["PrefabComponent"];
				if (prefabComponent)
				{
					// Add the prefab component to the deserialized entity.
					auto& pc = deserializedEntity.AddComponent<PrefabComponent>();
					pc.PrefabHandle = prefabComponent["PrefabHandle"].as<std::string>();

					// Retrieve the entire prefab hierarchy from the library.
					std::vector<Entity> prefabEntities = PrefabLibrary::GetEntities(pc.PrefabHandle);
					if (!prefabEntities.empty())
					{
						// Use the prefab root (first entity) to update the scene entity.
						Entity prefabRoot = prefabEntities.front();

						// Copy the prefab root's components to the deserialized entity.
						CopyComponents(deserializedEntity, prefabRoot);

						// Instantiate any children from the prefab hierarchy as children of deserializedEntity.
						InstantiatePrefabChildren(mScene.get(), deserializedEntity, prefabRoot);
					}
				}

				auto relationshipComponent = entity["RelationshipComponent"];
				if (relationshipComponent) 
				{
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

				auto spriteRendererComponent = entity["SpriteRendererComponent"];
				if (spriteRendererComponent)
				{
					auto& src = deserializedEntity.AddComponent<SpriteRendererComponent>();
					src.Color = spriteRendererComponent["Color"].as<DirectX::XMFLOAT4>();
				}

				auto planetComponent = entity["PlanetComponent"];
				if (planetComponent) 
				{
					auto& pc = deserializedEntity.AddComponent<PlanetComponent>(planetComponent["Subdivisions"].as<int16_t>(), planetComponent["MaxAltitude"].as<float>(), planetComponent["MinAltitude"].as<float>(), planetComponent["Radius"].as<float>(), planetComponent["GravitationalAcceleration"].as<float>(), planetComponent["SmoothShading"].as<bool>(), planetComponent["AtmosphereHeight"].as<float>(), planetComponent["AtmosphereToggle"].as<bool>(), planetComponent["InScatteringPoints"].as<int>(), planetComponent["OpticalDepthPoints"].as<int>(), planetComponent["MieAnisotropy"].as<float>(), planetComponent["RayScaleHeight"].as<float>(), planetComponent["MieScaleHeight"].as<float>(), planetComponent["RayBaseScatteringCoefficient"].as<DirectX::XMFLOAT3>(), planetComponent["MieBaseScatteringCoefficient"].as<float>(), planetComponent["SunDisc"].as<bool>(), planetComponent["SunDiscRadius"].as<float>(), planetComponent["SunGlowIntensity"].as<float>(), planetComponent["SunEdgeSoftness"].as<float>(), planetComponent["SunGlowSize"].as<float>());
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
					dlc.SunDesiredCoverage = directionalLightComponent["SunDesiredCoverage"].as<float>();
					dlc.SunLightDistance = directionalLightComponent["SunLightDistance"].as<float>();
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

					rbc.CenterOfMass = rigidBodyComponent["CenterOfMass"].as<Vector3>();
					rbc.InvMass = rigidBodyComponent["InvMass"].as<double>();
					rbc.Elasticity = rigidBodyComponent["Elasticity"].as<double>();
					rbc.Friction = rigidBodyComponent["Friction"].as<double>();
					rbc.LinearDamping = rigidBodyComponent["LinearDamping"].as<double>();
					rbc.AngularDamping = rigidBodyComponent["AngularDamping"].as<double>();
				}

				auto sphereColliderComponent = entity["SphereColliderComponent"];
				if (sphereColliderComponent)
				{
					auto& scc = deserializedEntity.AddComponent<SphereColliderComponent>();

					scc.Collider->mRadius = sphereColliderComponent["Radius"].as<float>();
					scc.RenderCollider = sphereColliderComponent["RenderCollider"].as<bool>();

					scc.Collider->CalculateBounds();
				}

				auto boxColliderComponent = entity["BoxColliderComponent"];
				if (boxColliderComponent)
				{
					auto& bcc = deserializedEntity.AddComponent<BoxColliderComponent>();

					bcc.Collider->mSize = boxColliderComponent["Size"].as<DirectX::XMFLOAT3>();
					bcc.RenderCollider = boxColliderComponent["RenderCollider"].as<bool>();

					bcc.Collider->CalculateBounds();
				}

				auto terrainColliderComponent = entity["TerrainColliderComponent"];
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

				auto uiPanelComponent = entity["UIPanelComponent"];
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
				}

				auto terrainDetailComponent = entity["TerrainDetailComponent"];
				if (terrainDetailComponent)
				{
					auto& tdc = deserializedEntity.AddComponent<TerrainDetailComponent>();

					if(terrainDetailComponent["Seed"].as<uint32_t>() != 0)
						tdc.Seed = terrainDetailComponent["Seed"].as<uint32_t>();
					tdc.SubdivisionActivation = terrainDetailComponent["SubdivisionActivation"].as<int>();
					tdc.Octaves = terrainDetailComponent["Octaves"].as<int>();
					tdc.Frequency = terrainDetailComponent["Frequency"].as<float>();
					tdc.Amplitude = terrainDetailComponent["Amplitude"].as<float>();
				}

				auto terrainObjectComponent = entity["TerrainObjectComponent"];
				if (terrainObjectComponent)
				{
					auto& toc = deserializedEntity.AddComponent<TerrainObjectComponent>();

					toc.MaxNrOfObjects = terrainObjectComponent["MaxNumberOfObjects"].as<int>();

					toc.MeshObject = CreateRef<Mesh>(terrainObjectComponent["AssetPath"].as<std::string>(), DirectX::XMFLOAT3(0.0, 0.0, 0.0), true, toc.MaxNrOfObjects);

					toc.SubdivisionActivation = terrainObjectComponent["SubdivisionActivation"].as<int>();
					toc.MaxNrOfObjectPerFace = terrainObjectComponent["MaxNumberOfObjectsPerFace"].as<int>();
				}

				auto particlesComponent = entity["ParticlesComponent"];
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