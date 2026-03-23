#include "tpch.h"
#include "SceneSerializer.h"

#include "Entity.h"
#include "Components.h"

#include "Toast/Scene/Prefab.h"

#include "Toast/Core/Math/Vector.h"

#include "Toast/Scripting/ScriptEngine.h"

#include "Toast/Physics/PhysicsEngine.h"

#include "Toast/Assets/AssetManager.h"

#include "Toast/Renderer/Renderer.h"
#include "Toast/Renderer/Renderer2D.h"

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

	class PhysicsEngine;

	SceneSerializer::SceneSerializer(Scene* scene)
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
			out << YAML::Key << "RotationQuat" << YAML::Value << tc.RotationQuaternion;
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

			out << YAML::Key << "Parts";
			out << YAML::Value << YAML::BeginSeq;
			auto& parts = mc.MeshObject->GetPartsUpdated();

			for (auto& part : parts)
			{
				out << YAML::BeginMap;
				out << YAML::Key << "Name" << YAML::Value << part.Name;
				out << YAML::Key << "Handle" << YAML::Value << part.EntityID;
				out << YAML::EndMap;
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
			out << YAML::Key << "StaticFriction" << YAML::Value << rbc.StaticFriction;
			out << YAML::Key << "DynamicFriction" << YAML::Value << rbc.DynamicFriction;
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

		if (entity.HasComponent<UIPanelComponent>())
		{
			out << YAML::Key << "UIPanelComponent";
			out << YAML::BeginMap; // UIPanelComponent

			auto& uipc = entity.GetComponent<UIPanelComponent>();
			out << YAML::Key << "Color" << YAML::Value << uipc.Color;
			out << YAML::Key << "CornerRadius" << YAML::Value << uipc.CornerRadius;
			out << YAML::Key << "TextureAssetHandle" << YAML::Value << uipc.TextureHandle;
			out << YAML::Key << "UseColor" << YAML::Value << uipc.UseColor;
			out << YAML::Key << "Visible" << YAML::Value << uipc.Visible;
			out << YAML::Key << "ConnectToParent" << YAML::Value << uipc.ConnectToParent;

			out << YAML::Key << "ConnectorColor" << YAML::Value << uipc.Connector.Color;
			out << YAML::Key << "ConnectorThickness" << YAML::Value << uipc.Connector.Thickness;
			out << YAML::Key << "ConnectorChildOffset" << YAML::Value << uipc.Connector.ChildOffset;
			out << YAML::Key << "ConnectorParentOffset" << YAML::Value << uipc.Connector.ParentOffset;

			out << YAML::EndMap; // UIPanelComponent
		}

		if (entity.HasComponent<UIButtonComponent>())
		{
			out << YAML::Key << "UIButtonComponent";
			out << YAML::BeginMap; // UIButtonComponent

			auto& ubc = entity.GetComponent<UIButtonComponent>();
			out << YAML::Key << "CornerRadius" << YAML::Value << ubc.CornerRadius;
			out << YAML::Key << "UseColor" << YAML::Value << ubc.UseColor;
			out << YAML::Key << "Color" << YAML::Value << ubc.Color;
			out << YAML::Key << "ClickColor" << YAML::Value << ubc.ClickColor;
			out << YAML::Key << "TextureAssetHandle" << YAML::Value << ubc.TextureHandle;
			out << YAML::Key << "ClickTextureAssetHandle" << YAML::Value << ubc.ClickTextureHandle;

			out << YAML::EndMap; // UIButtonComponent
		}

		if (entity.HasComponent<UITextComponent>())
		{
			out << YAML::Key << "UITextComponent";
			out << YAML::BeginMap; // UITextComponent

			auto& uitc = entity.GetComponent<UITextComponent>();
			out << YAML::Key << "AssetPath" << YAML::Value << uitc.Font->GetFilePath();
			out << YAML::Key << "Text" << YAML::Value << uitc.Text;
			out << YAML::Key << "TextureIndex" << YAML::Value << uitc.TextureIndex;
			out << YAML::Key << "Color" << YAML::Value << uitc.Color;

			out << YAML::EndMap; // UITextComponent
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
			out << YAML::Key << "MaskTextureAssetHandle" << YAML::Value << pc.MaskTextureHandle;
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
		CopyComponentIfExists<CameraComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<SpriteRendererComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<DirectionalLightComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<ScriptComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<RigidBodyComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<SphereColliderComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<BoxColliderComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<UIPanelComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<UITextComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<UIButtonComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);
		CopyComponentIfExists<ParticlesComponent>(target, target.GetScene()->mRegistry, source, source.GetScene()->mRegistry);

		if (target.HasComponent<RigidBodyComponent>() && (target.HasComponent<BoxColliderComponent>()))
		{
			auto& rbc = target.GetComponent<RigidBodyComponent>();
			auto& bcc = target.GetComponent<BoxColliderComponent>();

			bcc.Collider->CalculateInertiaTensor(1.0f / rbc.InvMass);
		}

		if (target.HasComponent<RigidBodyComponent>() && target.HasComponent<SphereColliderComponent>())
		{
			auto& rbc = target.GetComponent<RigidBodyComponent>();
			auto& scc = target.GetComponent<BoxColliderComponent>();

			scc.Collider->CalculateInertiaTensor(1.0f / rbc.InvMass);
		}
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

	void SceneSerializer::Serialize(const std::string& filepath, const std::string& name, Camera* editorCamera)
	{
		TOAST_CORE_TRACE("Serializing scene '%s'", name.c_str());

		YAML::Emitter out;
		out << YAML::BeginMap;

		out << YAML::Key << "Scene" << YAML::Value << name;

		out << YAML::Key << "EditorCamera";
		out << YAML::BeginMap;
		DirectX::XMFLOAT3 pos{ 0,0,0 };

		if (editorCamera)
			pos = editorCamera->GetTranslation();
		else if (auto* activeCamera = mScene->GetActiveCamera().get())
			pos = activeCamera->GetTranslation();
		else
			pos = DirectX::XMFLOAT3 { 0.0f, 1.0f, -3.0f };

		out << YAML::Key << "Position" << YAML::Value << pos;
		out << YAML::EndMap;

		Scene::Settings& settings = mScene->GetSettings();

		out << YAML::Key << "Settings";
		out << YAML::BeginMap;
		out << YAML::Key << "Wireframe" << YAML::Value << (int)settings.WireframeRendering;
		out << YAML::Key << "Grid" << YAML::Value << settings.Grid;
		out << YAML::Key << "CameraFrustum" << YAML::Value << settings.CameraFrustum;
		out << YAML::Key << "FrustumCullingMargin" << YAML::Value << settings.FrustumCullingMargin;
		out << YAML::Key << "DirectionalLightningGain" << YAML::Value << settings.DirectionalLightningGain;
		out << YAML::Key << "RenderColliders" << YAML::Value << settings.RenderColliders;
		out << YAML::Key << "RenderUI" << YAML::Value << settings.RenderUI;
		out << YAML::Key << "Shadows" << YAML::Value << settings.Shadows.Active;
		out << YAML::Key << "ShadowDistance" << YAML::Value << settings.Shadows.ShadowDistance;
		out << YAML::Key << "CascadeCount" << YAML::Value << settings.Shadows.CascadeCount;
		out << YAML::Key << "Lambda" << YAML::Value << settings.Shadows.Lambda;
		out << YAML::Key << "ConstantBias" << YAML::Value << settings.Shadows.ConstantBias;
		out << YAML::Key << "SlopeScaledBias" << YAML::Value << settings.Shadows.SlopeScaledBias;
		out << YAML::Key << "SSAO" << YAML::Value << settings.SSAO;
		out << YAML::Key << "SSAODebugging" << YAML::Value << settings.SSAODebugging;
		out << YAML::Key << "SSAORadius" << YAML::Value << settings.SSAORadius;
		out << YAML::Key << "SSAObias" << YAML::Value << settings.SSAObias;
		out << YAML::Key << "SunSurfaceThreshold" << YAML::Value << settings.Bloom.SunSurfaceThreshold;
		out << YAML::Key << "SunSurfaceIntensity" << YAML::Value << settings.Bloom.SunSurfaceIntensity;
		out << YAML::Key << "SunSpaceThreshold" << YAML::Value << settings.Bloom.SunSpaceThreshold;
		out << YAML::Key << "SunSpaceIntensity" << YAML::Value << settings.Bloom.SunSpaceIntensity;
		out << YAML::Key << "SkySurfaceThreshold" << YAML::Value << settings.Bloom.SkySurfaceThreshold;
		out << YAML::Key << "SkySurfaceIntensity" << YAML::Value << settings.Bloom.SkySurfaceIntensity;
		out << YAML::Key << "SkySpaceThreshold" << YAML::Value << settings.Bloom.SkySpaceThreshold;
		out << YAML::Key << "SkySpaceIntensity" << YAML::Value << settings.Bloom.SkySpaceIntensity;
		out << YAML::Key << "GeometryThreshold" << YAML::Value << settings.Bloom.GeometryThreshold;
		out << YAML::Key << "GeometryIntensity" << YAML::Value << settings.Bloom.GeometryIntensity;
		out << YAML::Key << "SunRadius" << YAML::Value << settings.Bloom.SunRadius;
		out << YAML::Key << "SkySurfaceRadius" << YAML::Value << settings.Bloom.SkySurfaceRadius;
		out << YAML::Key << "SkySpaceRadius" << YAML::Value << settings.Bloom.SkySpaceRadius;
		out << YAML::Key << "SoftKnee" << YAML::Value << settings.Bloom.SoftKnee;
		out << YAML::Key << "SaturationClamp" << YAML::Value << settings.Bloom.SaturationClamp;
		out << YAML::Key << "DynamicIBL" << YAML::Value << settings.DynamicIBL;
		out << YAML::Key << "SunFrustumOrthoSize" << YAML::Value << settings.SunFrustumOrthoSize;
		out << YAML::Key << "GodRaysExposure" << YAML::Value << settings.GodRays.Exposure;
		out << YAML::Key << "GodRaysDecay" << YAML::Value << settings.GodRays.Decay;
		out << YAML::Key << "GodRaysDensity" << YAML::Value << settings.GodRays.Density;
		out << YAML::Key << "GodRaysWeight" << YAML::Value << settings.GodRays.Weight;
		out << YAML::Key << "GodRaysKHalo" << YAML::Value << settings.GodRays.KHalo;
		out << YAML::Key << "GodRaysHaloPower" << YAML::Value << settings.GodRays.HaloPower;
		out << YAML::Key << "GodRaysFogRangeMeters" << YAML::Value << settings.GodRays.FogRangeMeters;

		out << YAML::Key << "EVSurfaceDay" << YAML::Value << settings.Exposure.EVSurfaceDay;
		out << YAML::Key << "EVSpaceDay" << YAML::Value << settings.Exposure.EVSpaceDay;
		out << YAML::Key << "EVSurfaceNight" << YAML::Value << settings.Exposure.EVSurfaceNight;
		out << YAML::Key << "EVSpaceNight" << YAML::Value << settings.Exposure.EVSpaceNight;
		out << YAML::Key << "AltFadeFrac" << YAML::Value << settings.Exposure.AltFadeFrac;
		out << YAML::Key << "SunFadeDeg" << YAML::Value << settings.Exposure.SunFadeDeg;

		out << YAML::Key << "Physics";
		out << YAML::BeginMap;
		PhysicsEngine::PhysicsSettings& physicsSettings = mScene->GetPhysicsEngine()->GetSettings();
		out << YAML::Key << "SlowDown" << YAML::Value << physicsSettings.SlowDown;
		out << YAML::Key << "FPSTarget" << YAML::Value << physicsSettings.FPSTarget;
		out << YAML::Key << "StepsPerUpdate" << YAML::Value << physicsSettings.StepsPerUpdate;
		out << YAML::Key << "MaxAngularVelocity" << YAML::Value << physicsSettings.MaxAngularVelocity;
		out << YAML::EndMap;

		out << YAML::EndMap;

		Planet* scenePlanet = mScene->GetPlanet().get();

		out << YAML::Key << "Planet";
		out << YAML::BeginMap;
		out << YAML::Key << "Translation" << YAML::Value << scenePlanet->mTranslation;
		out << YAML::Key << "Rotation" << YAML::Value << scenePlanet->mRotationEulerAngles;
		out << YAML::Key << "Radius" << YAML::Value << scenePlanet->mRadius;
		out << YAML::Key << "MaxHeight" << YAML::Value << scenePlanet->mMaxHeight;
		out << YAML::Key << "MinHeight" << YAML::Value << scenePlanet->mMinHeight;
		out << YAML::Key << "MeshMode" << YAML::Value << static_cast<uint32_t>(scenePlanet->mMeshMode);
		out << YAML::Key << "AlbedoTextureHandle" << YAML::Value << scenePlanet->mAlbedoTextureHandle;
		out << YAML::Key << "UseAlbedoMap" << YAML::Value << scenePlanet->mUseAlbedoMap;
		out << YAML::Key << "AlbedoColor" << YAML::Value << scenePlanet->mAlbedoColor;
		out << YAML::Key << "Roughness" << YAML::Value << scenePlanet->mRoughness;
		out << YAML::Key << "Metalness" << YAML::Value << scenePlanet->mMetalness;
		out << YAML::Key << "SlopeSensitivity" << YAML::Value << scenePlanet->mSlopeSensitivity;
		out << YAML::Key << "SlopeThreshold" << YAML::Value << scenePlanet->mSlopeThreshold;
		out << YAML::Key << "SlopeDarkening" << YAML::Value << scenePlanet->mSlopeDarkening;
		out << YAML::Key << "HeightMapAssetHandle" << YAML::Value << scenePlanet->mBaseHeightMapHandle;
		out << YAML::Key << "Metalness" << YAML::Value << scenePlanet->mMetalness;
		out << YAML::Key << "StarFieldAssetHandle" << YAML::Value << scenePlanet->mStarFieldTexture2DHandle;
		out << YAML::Key << "AtmosphereActivated" << YAML::Value << scenePlanet->mAtmosphereActivated;
		out << YAML::Key << "AtmosphereHeight" << YAML::Value << scenePlanet->mAtmosphere.AtmosphereHeight;
		out << YAML::Key << "RayleighScaleHeight" << YAML::Value << scenePlanet->mAtmosphere.RayleighScaleHeight;
		out << YAML::Key << "RayleighExp10" << YAML::Value << scenePlanet->mAtmosphere.RayleighExp10;
		out << YAML::Key << "RayleighScattering" << YAML::Value << scenePlanet->mAtmosphere.RayleighScattering;
		out << YAML::Key << "MieScaleHeight" << YAML::Value << scenePlanet->mAtmosphere.MieScaleHeight;
		out << YAML::Key << "MieScatteringExp10" << YAML::Value << scenePlanet->mAtmosphere.MieScatteringExp10;
		out << YAML::Key << "MieScattering" << YAML::Value << scenePlanet->mAtmosphere.MieScattering;
		out << YAML::Key << "MieAbsorptionExp10" << YAML::Value << scenePlanet->mAtmosphere.MieAbsorptionExp10;
		out << YAML::Key << "MieAbsorption" << YAML::Value << scenePlanet->mAtmosphere.MieAbsorption;
		out << YAML::Key << "MieAnisotropy" << YAML::Value << scenePlanet->mAtmosphere.MieAnisotropy;
		out << YAML::Key << "OzoneStrength" << YAML::Value << scenePlanet->mAtmosphere.OzoneStrength;
		out << YAML::Key << "GroundAlbedo" << YAML::Value << scenePlanet->mAtmosphere.GroundAlbedo;
		out << YAML::Key << "SunsetTint" << YAML::Value << scenePlanet->mAtmosphere.SunsetTint;
		out << YAML::Key << "MSGain" << YAML::Value << scenePlanet->mAtmosphere.MSGain;
		out << YAML::Key << "SGain" << YAML::Value << scenePlanet->mAtmosphere.SGain;
		out << YAML::Key << "GravityConstant" << YAML::Value << scenePlanet->mGravityConstant;

		auto& planetMeshGeo = scenePlanet->mGeoClipmapMesh;
		auto& planetMeshIco = scenePlanet->mIcosphereMesh;
		out << YAML::Key << "PlanetMesh";
		out << YAML::BeginMap;
		out << YAML::Key << "MaxSubdivisions" << YAML::Value << planetMeshIco->mMaxSubdivisions;
		out << YAML::Key << "PatchLevels" << YAML::Value << planetMeshIco->mPatchLevels;
		out << YAML::Key << "NearDistance" << YAML::Value << planetMeshIco->mNearDistance;
		out << YAML::Key << "FarDistance" << YAML::Value << planetMeshIco->mFarDistance;
		out << YAML::Key << "BackfaceCulling" << YAML::Value << planetMeshIco->mBackfaceCulling;
		out << YAML::Key << "FrustumCulling" << YAML::Value << planetMeshIco->mFrustumCulling;
		out << YAML::Key << "GridSize" << YAML::Value << planetMeshGeo->mGridSize;
		out << YAML::Key << "MaxLevels" << YAML::Value << planetMeshGeo->mNumLevels;
		out << YAML::EndMap;

		out << YAML::Key << "HeightDetails";
		out << YAML::BeginSeq;

		for (const HeightDetail& detail : scenePlanet->mHeightDetails)
		{
			out << YAML::BeginMap;
			out << YAML::Key << "Name" << YAML::Value << detail.Name;
			out << YAML::Key << "Seed" << YAML::Value << detail.Seed;
			out << YAML::Key << "LODActivation" << YAML::Value << detail.GPUSettings.LODActivation;
			out << YAML::Key << "Octaves" << YAML::Value << detail.GPUSettings.Octaves;
			out << YAML::Key << "Frequency" << YAML::Value << detail.GPUSettings.Frequency;
			out << YAML::Key << "Amplitude" << YAML::Value << detail.GPUSettings.Amplitude;
			out << YAML::EndMap;
		}

		out << YAML::EndSeq;

		out << YAML::Key << "TerrainObjects";
		out << YAML::BeginSeq;

		for (const TerrainObject& obj : scenePlanet->mTerrainObjects)
		{
			out << YAML::BeginMap;

			out << YAML::Key << "Name" << YAML::Value << obj.Name;
			out << YAML::Key << "Seed" << YAML::Value << obj.Seed;

			out << YAML::Key << "LODActivation" << YAML::Value << obj.LODActivation;

			out << YAML::Key << "DensityPerKm2" << YAML::Value << obj.DensityPerKm2;
			out << YAML::Key << "MaxPerPatch" << YAML::Value << obj.MaxPerPatch;
			out << YAML::Key << "MaxTotal" << YAML::Value << obj.MaxTotal;

			out << YAML::Key << "MinScale" << YAML::Value << obj.MinScale;
			out << YAML::Key << "MaxScale" << YAML::Value << obj.MaxScale;

			// Persist mesh asset path (or empty)
			std::string meshPath;
			if (obj.MeshObject && !obj.MeshObject->GetFilePath().empty())
				meshPath = obj.MeshObject->GetFilePath();
			else
				meshPath = ""; // or "Empty" if you prefer

			out << YAML::Key << "AssetPath" << YAML::Value << meshPath;

			out << YAML::EndMap;
		}

		out << YAML::EndSeq;

		out << YAML::EndMap;

		Scene::Environment& environment = mScene->GetEnvirontment();

		out << YAML::Key << "Environment";
		out << YAML::BeginMap;

		// Lightning Pass Gains
		out << YAML::Key << "DiffuseIBLGain" << YAML::Value << environment.DiffuseIBLGain;
		out << YAML::Key << "SpecularIBLGain" << YAML::Value << environment.SpecularIBLGain;

		// -------- Sun --------
		out << YAML::Key << "SunDiscToggle" << YAML::Value << environment.SunDiscToggle;
		out << YAML::Key << "SunIntensity" << YAML::Value << environment.SunIntensity;
		out << YAML::Key << "SunDiscRadius" << YAML::Value << environment.SunDiscRadius;      // radians
		out << YAML::Key << "SunEdgeSoftness" << YAML::Value << environment.SunEdgeSoftness;    // radians
		out << YAML::Key << "SunWhite" << YAML::Value << environment.SunWhite;
		out << YAML::Key << "WarmTint" << YAML::Value << environment.WarmTint;
		out << YAML::Key << "SpaceDiscBrightnessScale" << YAML::Value << environment.SpaceDiscBrightnessScale;

		// ---- Atmospheric Halo (in-air) ----
		out << YAML::Key << "AirHaloIntensity" << YAML::Value << environment.AirHaloIntensity;
		out << YAML::Key << "AirHaloStartFrac" << YAML::Value << environment.AirHaloStartFrac;
		out << YAML::Key << "AirHaloFalloffPow" << YAML::Value << environment.AirHaloFalloffPow;
		out << YAML::Key << "HorizonRefractionDeg" << YAML::Value << environment.HorizonRefractionDeg;
		out << YAML::Key << "TwilightBlendDeg" << YAML::Value << environment.TwilightBlendDeg;

		// -------- Space Halo --------
		out << YAML::Key << "SpaceHaloWidthDeg" << YAML::Value << environment.SpaceHaloWidthDeg;
		out << YAML::Key << "SpaceHaloIntensity" << YAML::Value << environment.SpaceHaloIntensity;
		out << YAML::Key << "SpaceHaloCutoffDeg" << YAML::Value << environment.SpaceHaloCutoffDeg;

		// -------- Diffraction Spikes --------
		out << YAML::Key << "SunSpikes" << YAML::Value << environment.SunSpikes;
		out << YAML::Key << "SunSpikeSharpness" << YAML::Value << environment.SunSpikeSharpness;
		out << YAML::Key << "SunSpikeRadiusSurface" << YAML::Value << environment.SunSpikeRadiusSurface;
		out << YAML::Key << "SunSpikeRadiusSpace" << YAML::Value << environment.SunSpikeRadiusSpace;
		out << YAML::Key << "SunSpikeFallOff" << YAML::Value << environment.SunSpikeFallOff;
		out << YAML::Key << "SunSpikeStrengthSurface" << YAML::Value << environment.SunSpikeStrengthSurface;
		out << YAML::Key << "SunSpikeStrengthSpace" << YAML::Value << environment.SunSpikeStrengthSpace;

		// -------- Sun Glare Veil --------
		out << YAML::Key << "SunGlareStrengthSurface" << YAML::Value << environment.SunGlareStrengthSurface;
		out << YAML::Key << "SunGlareStrengthSpace" << YAML::Value << environment.SunGlareStrengthSpace;
		out << YAML::Key << "SunGlareRadiusSurface" << YAML::Value << environment.SunGlareRadiusSurface;
		out << YAML::Key << "SunGlareRadiusSpace" << YAML::Value << environment.SunGlareRadiusSpace;

		// -------- General Lens and Ghost data --------
		out << YAML::Key << "LensAltStart" << YAML::Value << environment.LensAltStart;
		out << YAML::Key << "LensAltEnd" << YAML::Value << environment.LensAltEnd;
		out << YAML::Key << "GhostStrength" << YAML::Value << environment.GhostStrength;
		out << YAML::Key << "GhostSpacing" << YAML::Value << environment.GhostSpacing;
		out << YAML::Key << "GhostFalloff" << YAML::Value << environment.GhostFalloff;
		out << YAML::Key << "GhostSizeSurface" << YAML::Value << environment.GhostSizeSurface;
		out << YAML::Key << "GhostSizeSpace" << YAML::Value << environment.GhostSizeSpace;
		out << YAML::Key << "GhostAirSuppression" << YAML::Value << environment.GhostAirSuppression;

		// -------- Stars --------
		out << YAML::Key << "StarNits" << YAML::Value << environment.StarNits;
		out << YAML::Key << "TwilightStartDeg" << YAML::Value << environment.TwilightStartDeg;
		out << YAML::Key << "TwilightEndDeg" << YAML::Value << environment.TwilightEndDeg;
		out << YAML::Key << "SpaceFadeStart" << YAML::Value << environment.SpaceFadeStart;
		out << YAML::Key << "SpaceFadeEnd" << YAML::Value << environment.SpaceFadeEnd;

		// -------- Night Ambient --------
		out << YAML::Key << "NightAmbient" << YAML::Value << environment.NightAmbient;

		out << YAML::EndMap;

		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

		// Making sure the main camera is serialized first
		mScene->mRegistry.each([&](auto entityID)
		{
			Entity entity = { entityID, mScene };

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
			Entity entity = { entityID, mScene };

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

	bool SceneSerializer::Deserialize(const std::string& filepath, Camera* editorCamera)
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

		if (editorCamera && data["EditorCamera"]["Position"])
			editorCamera->SetTranslation(data["EditorCamera"]["Position"].as<DirectX::XMFLOAT3>());

		Scene::Settings& settings = mScene->GetSettings();

		settings.WireframeRendering = (Toast::Scene::Settings::Wireframe)data["Settings"]["Wireframe"].as<int>();
		settings.Grid = data["Settings"]["Grid"].as<bool>();
		settings.CameraFrustum = data["Settings"]["CameraFrustum"].as<bool>();
		settings.FrustumCullingMargin = data["Settings"]["FrustumCullingMargin"].as<float>();
		settings.DirectionalLightningGain = data["Settings"]["DirectionalLightningGain"].as<float>();
		settings.RenderColliders = data["Settings"]["RenderColliders"].as<bool>();
		settings.RenderUI = data["Settings"]["RenderUI"].as<bool>();
		settings.Shadows.Active = data["Settings"]["Shadows"].as<bool>();
		settings.Shadows.CascadeCount = data["Settings"]["CascadeCount"].as<uint32_t>();
		settings.Shadows.ShadowDistance = data["Settings"]["ShadowDistance"].as<float>();
		settings.Shadows.Lambda = data["Settings"]["Lambda"].as<float>();
		settings.Shadows.ConstantBias = data["Settings"]["ConstantBias"].as<float>();
		settings.Shadows.SlopeScaledBias = data["Settings"]["SlopeScaledBias"].as<float>();
		settings.SSAO = data["Settings"]["SSAO"].as<bool>();
		settings.SSAODebugging = data["Settings"]["SSAODebugging"].as<bool>();
		settings.SSAORadius = data["Settings"]["SSAORadius"].as<float>();
		settings.SSAObias = data["Settings"]["SSAObias"].as<float>();
		settings.Bloom.SunSurfaceThreshold = data["Settings"]["SunSurfaceThreshold"].as<float>();
		settings.Bloom.SunSurfaceIntensity = data["Settings"]["SunSurfaceIntensity"].as<float>();
		settings.Bloom.SunSpaceThreshold = data["Settings"]["SunSpaceThreshold"].as<float>();
		settings.Bloom.SunSpaceIntensity = data["Settings"]["SunSpaceIntensity"].as<float>();
		settings.Bloom.SkySurfaceThreshold = data["Settings"]["SkySurfaceThreshold"].as<float>();
		settings.Bloom.SkySurfaceIntensity = data["Settings"]["SkySurfaceIntensity"].as<float>();
		settings.Bloom.SkySpaceThreshold = data["Settings"]["SkySpaceThreshold"].as<float>();
		settings.Bloom.SkySpaceIntensity = data["Settings"]["SkySpaceIntensity"].as<float>();
		settings.Bloom.GeometryThreshold = data["Settings"]["GeometryThreshold"].as<float>();
		settings.Bloom.GeometryIntensity = data["Settings"]["GeometryIntensity"].as<float>();
		settings.Bloom.SunRadius = data["Settings"]["SunRadius"].as<float>();
		settings.Bloom.SkySurfaceRadius = data["Settings"]["SkySurfaceRadius"].as<float>();
		settings.Bloom.SkySpaceRadius = data["Settings"]["SkySpaceRadius"].as<float>();
		settings.Bloom.SoftKnee = data["Settings"]["SoftKnee"].as<float>();
		settings.Bloom.SaturationClamp = data["Settings"]["SaturationClamp"].as<float>();
		settings.DynamicIBL = data["Settings"]["DynamicIBL"].as<bool>();
		settings.SunFrustumOrthoSize = data["Settings"]["SunFrustumOrthoSize"].as<float>();
		settings.GodRays.Exposure = data["Settings"]["GodRaysExposure"].as<float>();
		settings.GodRays.Decay = data["Settings"]["GodRaysDecay"].as<float>();
		settings.GodRays.Density = data["Settings"]["GodRaysDensity"].as<float>();
		settings.GodRays.Weight = data["Settings"]["GodRaysWeight"].as<float>();
		settings.GodRays.KHalo = data["Settings"]["GodRaysKHalo"].as<float>();
		settings.GodRays.HaloPower = data["Settings"]["GodRaysHaloPower"].as<float>();
		settings.GodRays.FogRangeMeters = data["Settings"]["GodRaysFogRangeMeters"].as<float>();
		settings.Exposure.EVSurfaceDay = data["Settings"]["EVSurfaceDay"].as<float>();
		settings.Exposure.EVSpaceDay = data["Settings"]["EVSpaceDay"].as<float>();
		settings.Exposure.EVSurfaceNight = data["Settings"]["EVSurfaceNight"].as<float>();
		settings.Exposure.EVSpaceNight = data["Settings"]["EVSpaceNight"].as<float>();
		settings.Exposure.AltFadeFrac = data["Settings"]["AltFadeFrac"].as<DirectX::XMFLOAT2>();
		settings.Exposure.SunFadeDeg = data["Settings"]["SunFadeDeg"].as<DirectX::XMFLOAT2>();

		PhysicsEngine::PhysicsSettings& physicsSettings = mScene->GetPhysicsEngine()->GetSettings();

		physicsSettings.SlowDown = data["Settings"]["Physics"]["SlowDown"].as<int>();
		physicsSettings.FPSTarget = data["Settings"]["Physics"]["FPSTarget"].as<int>();
		physicsSettings.StepsPerUpdate = data["Settings"]["Physics"]["StepsPerUpdate"].as<int>();
		physicsSettings.MaxAngularVelocity = data["Settings"]["Physics"]["MaxAngularVelocity"].as<float>();

		Planet* scenePlanet = mScene->GetPlanet().get();


		//std::string absPath = planet["HeightMapAssetPath"].as<std::string>();
		//scenePlanet->mBaseHeightMapHandle = AssetManager::ImportExternalAsset(
		//	absPath, "textures");

		auto planet = data["Planet"];
		scenePlanet->mTranslation = planet["Translation"].as<DirectX::XMFLOAT3>();
		scenePlanet->mRotationEulerAngles = planet["Rotation"].as<DirectX::XMFLOAT3>();
		scenePlanet->mRadius = planet["Radius"].as<double>();
		scenePlanet->mMaxHeight = planet["MaxHeight"].as<double>();
		scenePlanet->mMinHeight = planet["MinHeight"].as<double>();
		scenePlanet->mMeshMode = static_cast<PlanetMeshMode>(planet["MeshMode"].as<uint32_t>());
		scenePlanet->mAlbedoTextureHandle = planet["AlbedoTextureHandle"].as<AssetHandle>();
		scenePlanet->mUseAlbedoMap = planet["UseAlbedoMap"].as<float>();
		scenePlanet->mAlbedoColor = planet["AlbedoColor"].as<DirectX::XMFLOAT3>();
		scenePlanet->mRoughness = planet["Roughness"].as<float>();
		scenePlanet->mMetalness = planet["Metalness"].as<float>();
		scenePlanet->mSlopeSensitivity = planet["SlopeSensitivity"].as<float>();
		scenePlanet->mSlopeThreshold = planet["SlopeThreshold"].as<float>();
		scenePlanet->mSlopeDarkening = planet["SlopeDarkening"].as<float>();
		scenePlanet->mBaseHeightMapHandle = planet["HeightMapAssetHandle"].as<AssetHandle>();
		scenePlanet->mBaseHeightMapTextureCube = scenePlanet->CreateHeightMapCube(AssetManager::GetAsset<Texture2D>(scenePlanet->mBaseHeightMapHandle).get());
		scenePlanet->mStarFieldTexture2DHandle = planet["StarFieldAssetHandle"].as<AssetHandle>();
		scenePlanet->mAtmosphereActivated = planet["AtmosphereActivated"].as<bool>();
		scenePlanet->mAtmosphere.AtmosphereHeight = planet["AtmosphereHeight"].as<float>();
		scenePlanet->mAtmosphere.RayleighScaleHeight = planet["RayleighScaleHeight"].as<float>();
		scenePlanet->mAtmosphere.RayleighExp10 = planet["RayleighExp10"].as<int>();
		scenePlanet->mAtmosphere.RayleighScattering = planet["RayleighScattering"].as<DirectX::XMFLOAT3>();
		scenePlanet->mAtmosphere.MieScaleHeight = planet["MieScaleHeight"].as<float>();
		scenePlanet->mAtmosphere.MieScatteringExp10 = planet["MieScatteringExp10"].as<int>();
		scenePlanet->mAtmosphere.MieScattering = planet["MieScattering"].as<DirectX::XMFLOAT3>();
		scenePlanet->mAtmosphere.MieAbsorptionExp10 = planet["MieAbsorptionExp10"].as<int>();
		scenePlanet->mAtmosphere.MieAbsorption = planet["MieAbsorption"].as<DirectX::XMFLOAT3>();
		scenePlanet->mAtmosphere.MieAnisotropy = planet["MieAnisotropy"].as<DirectX::XMFLOAT3>();
		scenePlanet->mAtmosphere.OzoneStrength = planet["OzoneStrength"].as<float>();
		scenePlanet->mAtmosphere.GroundAlbedo = planet["GroundAlbedo"].as<DirectX::XMFLOAT3>();
		scenePlanet->mAtmosphere.SunsetTint = planet["SunsetTint"].as<DirectX::XMFLOAT3>();
		scenePlanet->mAtmosphere.MSGain = planet["MSGain"].as<float>();
		scenePlanet->mAtmosphere.SGain = planet["SGain"].as<float>();
		scenePlanet->mGravityConstant = planet["GravityConstant"].as<float>();

		auto& planetMeshGeo = scenePlanet->mGeoClipmapMesh;
		planetMeshGeo->mGridSize = planet["PlanetMesh"]["GridSize"].as<uint32_t>();
		planetMeshGeo->mNumLevels = planet["PlanetMesh"]["MaxLevels"].as<uint32_t>();
		planetMeshGeo->mGridIsDirty = true;
		planetMeshGeo->mLODGridIsDirty = true;
		planetMeshGeo->mRingGridIsDirty = true;

		auto& planetMeshIco = scenePlanet->mIcosphereMesh;
		planetMeshIco->mMaxSubdivisions = planet["PlanetMesh"]["MaxSubdivisions"].as<int>();
		planetMeshIco->mPatchLevels = planet["PlanetMesh"]["PatchLevels"].as<int>();
		planetMeshIco->mNearDistance = planet["PlanetMesh"]["NearDistance"].as<double>();
		planetMeshIco->mFarDistance = planet["PlanetMesh"]["FarDistance"].as<double>();
		planetMeshIco->mBackfaceCulling = planet["PlanetMesh"]["BackfaceCulling"].as<bool>();
		planetMeshIco->mFrustumCulling = planet["PlanetMesh"]["FrustumCulling"].as<bool>();
		planetMeshIco->mDistanceLUTIsDirty = true;
		planetMeshIco->mFaceLevelDotLUTIsDirty = true;
		planetMeshIco->mHeightMultLUTIsDirty = true;
		planetMeshIco->mPatchIsDirty = true;

		scenePlanet->mNormalMapTextureCube = scenePlanet->CreateNormalMapCube(scenePlanet->mBaseHeightMapTextureCube.get());
		if(scenePlanet->mUseAlbedoMap > 0.0f)
			scenePlanet->mAlbedoMapTextureCube = scenePlanet->CreateAlbedoCube(AssetManager::GetAsset<Texture2D>(scenePlanet->mAlbedoTextureHandle).get());

		scenePlanet->mHeightDetails.clear();

		YAML::Node heightDetailsNode = planet["HeightDetails"];
		if (heightDetailsNode && heightDetailsNode.IsSequence())
		{
			for (const YAML::Node& node : heightDetailsNode)
			{
				HeightDetail detail;

				detail.Name = node["Name"].as<std::string>();
				detail.Seed = node["Seed"].as<uint32_t>();

				detail.GPUSettings.LODActivation = node["LODActivation"].as<int>();
				detail.GPUSettings.Octaves = node["Octaves"].as<int>();
				detail.GPUSettings.Frequency = node["Frequency"].as<float>();
				detail.GPUSettings.Amplitude = node["Amplitude"].as<float>();

				scenePlanet->mHeightDetails.emplace_back(std::move(detail));
			}
		}

		scenePlanet->UploadHeightDetailsToGPU();

		Renderer::ResetEnvMapsIBLDone();

		scenePlanet->mTerrainObjects.clear();

		YAML::Node terrainObjectsNode = planet["TerrainObjects"];
		if (terrainObjectsNode && terrainObjectsNode.IsSequence())
		{
			for (const YAML::Node& node : terrainObjectsNode)
			{
				TerrainObject obj;

				obj.Name = node["Name"].as<std::string>();
				obj.Seed = node["Seed"].as<uint32_t>();

				obj.LODActivation = node["LODActivation"].as<int>();

				obj.DensityPerKm2 = node["DensityPerKm2"].as<float>();
				obj.MaxPerPatch = node["MaxPerPatch"].as<int>();
				obj.MaxTotal = node["MaxTotal"].as<int>();

				obj.MinScale = node["MinScale"].as<float>();
				obj.MaxScale = node["MaxScale"].as<float>();

				// Mesh (optional / nullable)
				if (YAML::Node meshPathNode = node["AssetPath"])
				{
					std::string assetPath = meshPathNode.as<std::string>();
					if (!assetPath.empty() && assetPath != "Empty")
						obj.MeshObject = CreateRef<Mesh>(assetPath);
				}

				scenePlanet->mTerrainObjects.emplace_back(std::move(obj));
			}
		}

		Scene::Environment& environment = mScene->GetEnvirontment();

		auto env = data["Environment"];
		environment.DiffuseIBLGain = env["DiffuseIBLGain"].as<float>();
		environment.SpecularIBLGain = env["SpecularIBLGain"].as<float>();
		environment.SunDiscToggle = env["SunDiscToggle"].as<bool>();
		environment.SunIntensity = env["SunIntensity"].as<float>();
		environment.SunDiscRadius = env["SunDiscRadius"].as<float>();
		environment.SunEdgeSoftness = env["SunEdgeSoftness"].as<float>();
		environment.StarNits = env["StarNits"].as<float>();
		environment.TwilightStartDeg = env["TwilightStartDeg"].as<float>();
		environment.TwilightEndDeg = env["TwilightEndDeg"].as<float>();
		environment.SpaceFadeStart = env["SpaceFadeStart"].as<float>();
		environment.SpaceFadeEnd = env["SpaceFadeEnd"].as<float>();
		environment.NightAmbient = env["NightAmbient"].as<DirectX::XMFLOAT3>();

		environment.SunWhite = env["SunWhite"].as<DirectX::XMFLOAT3>();
		environment.SpaceDiscBrightnessScale = env["SpaceDiscBrightnessScale"].as<float>();
		environment.WarmTint = env["WarmTint"].as<DirectX::XMFLOAT3>();
		environment.AirHaloIntensity = env["AirHaloIntensity"].as<float>();

		environment.AirHaloStartFrac = env["AirHaloStartFrac"].as<float>();
		environment.AirHaloFalloffPow = env["AirHaloFalloffPow"].as<float>();
		environment.HorizonRefractionDeg = env["HorizonRefractionDeg"].as<float>();
		environment.TwilightBlendDeg = env["TwilightBlendDeg"].as<float>();

		environment.SpaceHaloWidthDeg = env["SpaceHaloWidthDeg"].as<float>();
		environment.SpaceHaloIntensity = env["SpaceHaloIntensity"].as<float>();
		environment.SpaceHaloCutoffDeg = env["SpaceHaloCutoffDeg"].as<float>();

		// -------- Diffraction Spikes --------
		environment.SunSpikes = env["SunSpikes"].as<int>(); 
		environment.SunSpikeSharpness = env["SunSpikeSharpness"].as<float>();
		environment.SunSpikeRadiusSurface = env["SunSpikeRadiusSurface"].as<float>();
		environment.SunSpikeRadiusSpace = env["SunSpikeRadiusSpace"].as<float>();
		environment.SunSpikeFallOff = env["SunSpikeFallOff"].as<float>();
		environment.SunSpikeStrengthSurface = env["SunSpikeStrengthSurface"].as<float>();
		environment.SunSpikeStrengthSpace = env["SunSpikeStrengthSpace"].as<float>();

		// -------- Sun Glare Veil --------
		environment.SunGlareStrengthSurface = env["SunGlareStrengthSurface"].as<float>();
		environment.SunGlareStrengthSpace = env["SunGlareStrengthSpace"].as<float>();
		environment.SunGlareRadiusSurface = env["SunGlareRadiusSurface"].as<float>();
		environment.SunGlareRadiusSpace = env["SunGlareRadiusSpace"].as<float>();

		// -------- General Lens and Ghost data --------
		environment.LensAltStart = env["LensAltStart"].as<float>();
		environment.LensAltEnd = env["LensAltEnd"].as<float>();
		environment.GhostStrength = env["GhostStrength"].as<float>();
		environment.GhostSpacing = env["GhostSpacing"].as<float>();
		environment.GhostFalloff = env["GhostFalloff"].as<float>();
		environment.GhostSizeSurface = env["GhostSizeSurface"].as<float>();
		environment.GhostSizeSpace = env["GhostSizeSpace"].as<float>();
		environment.GhostAirSuppression = env["GhostAirSuppression"].as<float>();

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
						InstantiatePrefabChildren(mScene, deserializedEntity, prefabRoot);
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
					tc.RotationQuaternion = transformComponent["RotationQuat"].as<DirectX::XMFLOAT4>();
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

					if (cc.Primary)
						mScene->SetMainCamera(&cc.Camera);
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

					if (meshComponent["Parts"])
					{
						const YAML::Node& partsNode = meshComponent["Parts"];
						auto& parts = mc.MeshObject->GetPartsUpdated();

						for (std::size_t i = 0; i < partsNode.size(); ++i)
						{
							const auto& partNode = partsNode[i];
							std::string name = partNode["Name"].as<std::string>();
							UUID handle = partNode["Handle"].as<UUID>();

							for (auto& part : parts)
							{
								if (part.Name == name)
								{
									part.EntityID = handle;
									break;
								}
							}
						}
					}
				}

				auto spriteRendererComponent = entity["SpriteRendererComponent"];
				if (spriteRendererComponent)
				{
					auto& src = deserializedEntity.AddComponent<SpriteRendererComponent>();
					src.Color = spriteRendererComponent["Color"].as<DirectX::XMFLOAT4>();
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
					rbc.StaticFriction = rigidBodyComponent["StaticFriction"].as<double>();
					rbc.DynamicFriction = rigidBodyComponent["DynamicFriction"].as<double>();
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

					scc.InertiaTensorIsDirty = true;
				}

				auto boxColliderComponent = entity["BoxColliderComponent"];
				if (boxColliderComponent)
				{
					auto& bcc = deserializedEntity.AddComponent<BoxColliderComponent>();

					bcc.Collider->mSize = boxColliderComponent["Size"].as<DirectX::XMFLOAT3>();
					bcc.RenderCollider = boxColliderComponent["RenderCollider"].as<bool>();

					bcc.Collider->CalculateBounds();
					bcc.Collider->BuildCornerPoints();

					bcc.InertiaTensorIsDirty = true;

				}

				auto uiPanelComponent = entity["UIPanelComponent"];
				if (uiPanelComponent)
				{
					auto& tc = deserializedEntity.GetComponent<TransformComponent>();
					auto& uipc = deserializedEntity.AddComponent<UIPanelComponent>();
					
					uipc.Color = uiPanelComponent["Color"].as<DirectX::XMFLOAT4>();
					uipc.CornerRadius = uiPanelComponent["CornerRadius"].as<float>();
					uipc.UseColor = uiPanelComponent["UseColor"].as<bool>();
					uipc.Visible = uiPanelComponent["Visible"].as<bool>();
					uipc.ConnectToParent = uiPanelComponent["ConnectToParent"].as<bool>();

					uipc.Connector.Color = uiPanelComponent["ConnectorColor"].as<DirectX::XMFLOAT4>();
					uipc.Connector.Thickness = uiPanelComponent["ConnectorThickness"].as<float>();
					uipc.Connector.ChildOffset = uiPanelComponent["ConnectorChildOffset"].as<DirectX::XMFLOAT2>();
					uipc.Connector.ParentOffset = uiPanelComponent["ConnectorParentOffset"].as<DirectX::XMFLOAT2>();

					uipc.TextureHandle = uiPanelComponent["TextureAssetHandle"].as<AssetHandle>();
					uipc.TextureIndex = Renderer2D::GetRendererData()->UITextureArray->GetSliceIndexForHandle(uipc.TextureHandle);
					TOAST_CORE_CRITICAL("TEXTURE INDEX FOR %llu is %d", uipc.TextureHandle, uipc.TextureIndex);
				}

				auto uiButtonComponent = entity["UIButtonComponent"];
				if (uiButtonComponent)
				{
					auto& ubc = deserializedEntity.AddComponent<UIButtonComponent>();

					ubc.Color = uiButtonComponent["Color"].as<DirectX::XMFLOAT4>();
					ubc.UseColor = uiButtonComponent["UseColor"].as<bool>();
					ubc.ClickColor = uiButtonComponent["ClickColor"].as<DirectX::XMFLOAT4>();
					ubc.CornerRadius = uiButtonComponent["CornerRadius"].as<float>();

					ubc.TextureHandle = uiButtonComponent["TextureAssetHandle"].as<AssetHandle>();
					ubc.TextureIndex = Renderer2D::GetRendererData()->UITextureArray->GetSliceIndexForHandle(ubc.TextureHandle);

					ubc.ClickTextureHandle = uiButtonComponent["ClickTextureAssetHandle"].as<AssetHandle>();
					ubc.ClickTextureIndex = Renderer2D::GetRendererData()->UITextureArray->GetSliceIndexForHandle(ubc.ClickTextureHandle);
				}

				auto uiTextComponent = entity["UITextComponent"];
				if (uiTextComponent)
				{
					auto& uitc = deserializedEntity.AddComponent<UITextComponent>();

					uitc.Font = CreateRef<Font>(uiTextComponent["AssetPath"].as<std::string>());
					uitc.Text = uiTextComponent["Text"].as<std::string>();
					uitc.Color = uiTextComponent["Color"].as<DirectX::XMFLOAT4>();

					if (uiTextComponent["TextureIndex"])
						uitc.TextureIndex = uiTextComponent["TextureIndex"].as<int>();
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

					pc.MaskTextureHandle = particlesComponent["MaskTextureAssetHandle"].as<AssetHandle>();
				}
			}
		}

		if (planetMeshGeo->mNumLevels != 0 && planetMeshGeo->mGridSize != 0)
			planetMeshGeo->Init();

			if (scenePlanet->mStarFieldTexture2DHandle != AssetHandle(0))
			{
				scenePlanet->mStarFieldTextureCube = Renderer::CreateStarFieldTexture(AssetManager::GetAsset<Texture2D>(scenePlanet->mStarFieldTexture2DHandle).get());

				scenePlanet->mStarFieldTextureCube->GenerateMips();
			}

			planetMeshGeo->mTempGridSize = planetMeshGeo->mGridSize;
			planetMeshGeo->mTempNumLevels = planetMeshGeo->mNumLevels;

			scenePlanet->mTerrainCubeData = scenePlanet->LoadTerrainDataFromTextureCube();

			SceneCamera* camera = mScene->GetMainCamera();
			if (camera)
				planetMeshGeo->GenerateDistanceLUT(planetMeshGeo->mNumLevels, scenePlanet->mRadius, camera->GetPerspectiveVerticalFOV(), std::get<0>(mScene->GetViewportSize()));

				Renderer::GenerateTransmittanceLUT(scenePlanet);
				Renderer::GenerateMultiScatteringLUT(scenePlanet);
		//}

		return true;
	}

	bool SceneSerializer::DeserializeRuntime(const std::string& filepath)
	{
		// Not implemented yet
		TOAST_CORE_ASSERT(false, "Not implemented yet!");
		return false;
	}

}