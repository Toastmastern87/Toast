#pragma once

#include <DirectXMath.h>

#include "Toast/Core/UUID.h"
#include "Toast/Core/Math/Math.h"

#include "Toast/Scene/SceneCamera.h"

#include "Toast/Renderer/Mesh.h"

#include "Toast/Renderer/UI/Font.h"

#include "Toast/Physics/Bounds.h"
#include "Toast/Physics/Shapes.h"

#include "Toast/Assets/AssetManager.h"

#include <../vendor/directxtex/include/DirectXTex.h>
#include <mutex>

#include "../vendor/robinhood/include/robin_hood.h"

namespace Toast {

	// Forward deceleration, PlanetNode is found in PlanetSystem.cpp
	struct PlanetNode;

	// Forward deceleration, Particle is found in ParticleSystem.h
	struct Particle;

	// Forward deceleration, EmitFunction is found in ParticleSystem.h
	enum class EmitFunction;

	struct PairHash {
		std::size_t operator()(const std::pair<int, int>& p) const {
			return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
		}
	};

	struct IDComponent
	{
		UUID ID = 0;
	};

	struct TagComponent
	{
		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(const std::string& tag)
			: Tag(tag) {}
	};

	struct RelationshipComponent
	{
		UUID ParentHandle = 0;
		std::vector<UUID> Children;

		RelationshipComponent() = default;
		RelationshipComponent(const RelationshipComponent& other) = default;
		RelationshipComponent(UUID parent)
			: ParentHandle(parent) {}
	};

	struct PrefabComponent
	{
		std::string PrefabHandle;

		PrefabComponent() = default;
		PrefabComponent(const PrefabComponent& other) = default;
	};


	struct TransformComponent
	{
		bool IsDirty = false;

		DirectX::XMFLOAT3 Translation = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 RotationEulerAngles = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT4 RotationQuaternion = { 0.0f, 0.0f, 0.0f, 1.0f };
		DirectX::XMFLOAT3 Scale = { 1.0f, 1.0f, 1.0f };

		// Variables used for animating rotation, not serialized
		DirectX::XMFLOAT4 TargetRotationQuaternion = { 0.0f, 0.0f, 0.0f, 1.0f };
		float AngularSpeed = 0.0f;
		bool IsRotating = false;

		DirectX::XMFLOAT3 Up = { 0.0f, 1.0f, 0.0f };
		DirectX::XMFLOAT3 Right = { 1.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 Forward = { 0.0f, 0.0f, 1.0f };

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;

		DirectX::XMMATRIX GetTransform() 
		{
			return DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(Scale.x, Scale.y, Scale.z)
				* (DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(RotationEulerAngles.x), DirectX::XMConvertToRadians(RotationEulerAngles.y), DirectX::XMConvertToRadians(RotationEulerAngles.z)))) * DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&RotationQuaternion))
				* DirectX::XMMatrixTranslation(Translation.x, Translation.y, Translation.z);
		}

		DirectX::XMMATRIX GetTransformWithoutScale()
		{
			return DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(RotationEulerAngles.x), DirectX::XMConvertToRadians(RotationEulerAngles.y), DirectX::XMConvertToRadians(RotationEulerAngles.z))) * DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&RotationQuaternion)) * DirectX::XMMatrixTranslation(Translation.x, Translation.y, Translation.z);
		}

		DirectX::XMMATRIX GetRotation()
		{
			return DirectX::XMMatrixIdentity() * (DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(RotationEulerAngles.x), DirectX::XMConvertToRadians(RotationEulerAngles.y), DirectX::XMConvertToRadians(RotationEulerAngles.z)))) * DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&RotationQuaternion));
		}


		DirectX::XMVECTOR GetTotalRotationQuaternion() const
		{
			DirectX::XMVECTOR qEuler = DirectX::XMQuaternionRotationRollPitchYaw(DirectX::XMConvertToRadians(RotationEulerAngles.x), DirectX::XMConvertToRadians(RotationEulerAngles.y), DirectX::XMConvertToRadians(RotationEulerAngles.z));

			DirectX::XMVECTOR qExtra = DirectX::XMLoadFloat4(&RotationQuaternion);

			// Must match GetRotation() matrix order
			DirectX::XMVECTOR qTotal = DirectX::XMQuaternionMultiply(qEuler, qExtra);

			return DirectX::XMQuaternionNormalize(qTotal);
		}
	};

	struct MeshComponent
	{
		Ref<Mesh> MeshObject;

		MeshComponent() = default;
		MeshComponent(const MeshComponent& other) = default;
		MeshComponent(const Ref<Mesh>& mesh)
			: MeshObject(mesh) {}

		operator Ref<Mesh>() { return MeshObject; }
	};

	struct MeshPartComponent
	{
		bool TempValue;

		MeshPartComponent() = default;
		MeshPartComponent(const MeshPartComponent& other) = default;
		MeshPartComponent(const bool& value)
			: TempValue(value) {
		}
	};

	struct SpriteRendererComponent
	{
		DirectX::XMFLOAT4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const DirectX::XMFLOAT4& color)
			: Color(color) {}
	};

	struct CameraComponent
	{
		SceneCamera Camera;
		bool Primary = true;
		bool FixedAspectRatio = false;
		bool IsDirty = false;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
	};

	struct ScriptComponent
	{
		std::string ClassName;

		ScriptComponent() = default;
		ScriptComponent(const ScriptComponent&) = default;
		ScriptComponent(const std::string& className)
			: ClassName(className) {}
	};

	struct DirectionalLightComponent
	{
		DirectX::XMFLOAT3 Radiance = { 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;
		float SunDesiredCoverage = 0.0f;
		float SunLightDistance = 0.0f;
	};

	struct RigidBodyComponent
	{
		bool IsStatic = false;
		double InvMass = 0.0f;
		double Elasticity = 0.0f;
		double StaticFriction = 0.0f;
		double DynamicFriction = 0.0f;
		Vector3 CenterOfMass = { 0.0f, 0.0f, 0.0f };
		Vector3 LinearVelocity = { 0.0f, 0.0f, 0.0f };
		Vector3 AngularVelocity = { 0.0f, 0.0f, 0.0f };
		double AngularDamping = 0.0f;
		double Altitude = 0.0;

		Matrix InertiaTensor;
		Matrix InvInertiaTensor;

		float DragCoefficient = 0.0f; // Cd, 0 means no drag
		float CrossSectionMin = 0.0f; // m^2, smallest profile
		float CrossSectionMax = 0.0f; // m^2, largest profile

		// Temp debug values for Air Resistance
		Vector3 DebugDragForce = Vector3(0.0, 0.0, 0.0);
		double DebugAirDensity = 0.0;  

		double DebugEffectiveCrossSection = 0.0;
		double DebugAltitude = 0.0;

		Ref<Mesh> GuideMesh;

		RigidBodyComponent() = default;
		RigidBodyComponent(Vector3& centerOfMass, double invMass)
			: InvMass(invMass), CenterOfMass(centerOfMass) {}
	};

	struct SphereColliderComponent
	{
		Ref<ShapeSphere> Collider;
		bool IsDirty = false;

		Ref<Mesh> ColliderMesh;
		bool RenderCollider = false;

		SphereColliderComponent() = default;
		SphereColliderComponent(Ref<ShapeSphere>& collider, const Ref<Mesh>& mesh)
			: Collider(collider), ColliderMesh(mesh) {}
	};

	struct BoxColliderComponent
	{
		Ref<ShapeBox> Collider;
		bool IsDirty = false;

		Ref<Mesh> ColliderMesh;
		bool RenderCollider = false;

		BoxColliderComponent() = default;
		BoxColliderComponent(Ref<ShapeBox>& collider)
			: Collider(collider) {}
	};

	struct UIPanelComponent
	{
		struct UIConnector
		{
			UIConnector() = default;

			DirectX::XMFLOAT2 ParentOffset = { 0.0f, 0.0f };
			DirectX::XMFLOAT2 ChildOffset = { 0.0f, 0.0f };
			DirectX::XMFLOAT4 Color = { 0.0f, 0.0f, 0.0f, 0.0f };
			float Thickness = 1.0f;
		};

		bool Visible = false;
		AssetHandle	TextureHandle = 0;
		uint32_t TextureIndex = 0;

		DirectX::XMFLOAT4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };

		float CornerRadius = 0.0f;
		bool UseColor = true;
		bool ConnectToParent = false;

		UIConnector Connector;

		UIPanelComponent() = default;
		UIPanelComponent(const UIPanelComponent&) = default;
	};

	struct UITextComponent 
	{
		bool Visible = false;
		uint32_t TextureIndex = 0;

		DirectX::XMFLOAT4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };

		std::string Text = "Enter Text here";
		Ref<Font> Font = Font::GetDefaultFont();

		UITextComponent() = default;
		UITextComponent(const UITextComponent&) = default;
	};

	struct UIButtonComponent 
	{
		bool Visible = false;
		float CornerRadius = 0.0f;
		bool UseColor = true;
		bool IsClicked = false;

		DirectX::XMFLOAT4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		DirectX::XMFLOAT4 ClickColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		AssetHandle	TextureHandle = 0;
		uint32_t TextureIndex = 0;
		AssetHandle	ClickTextureHandle = 0;
		uint32_t ClickTextureIndex = 0;

		UIButtonComponent() = default;
		UIButtonComponent(const UIButtonComponent&) = default;
	};

	struct ParticlesComponent
	{
		bool Emitting;
		Ref<Mesh> GuideMesh;
		std::vector<Particle> Particles;
		float Size = 0.0f;
		float MaxLifeTime; // in seconds
		float SpawnDelay = 1.0f; // Delay between particle spawn
		DirectX::XMFLOAT3 Velocity = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 StartColor = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 EndColor = { 0.0f, 0.0f, 0.0f };
		float ColorBlendFactor = 0.0f;
		float ConeAngleDegrees = 0.0;
		float BiasExponent = 1.0f;
		float GrowRate = 0.0f;
		float BurstInitial = 1.0f;
		float BurstDecay = 0.0f;

		float ElapsedTime;

		EmitFunction SpawnFunction;

		AssetHandle MaskTextureHandle;

		ParticlesComponent() = default;
		ParticlesComponent(const ParticlesComponent& other) = default;
	};

}