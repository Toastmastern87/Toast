#pragma once

#include <DirectXMath.h>

#include "Toast/Core/UUID.h"
#include "Toast/Core/Math/Math.h"

#include "Toast/Scene/SceneCamera.h"

#include "Toast/Renderer/Mesh.h"

#include "Toast/Renderer/UI/UIElement.h"

#include "Toast/Physics/Bounds.h"
#include "Toast/Physics/Shapes.h"

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
		DirectX::XMFLOAT3 Scale = { 1.0f, 1.0f, 1.0f };

		DirectX::XMFLOAT4 RotationQuaternion = { 0.0f, 0.0f, 0.0f, 1.0f };

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
		double Friction = 0.0f;
		Vector3 CenterOfMass = { 0.0f, 0.0f, 0.0f };
		Vector3 LinearVelocity = { 0.0f, 0.0f, 0.0f };
		double LinearDamping = 0.0;
		Vector3 AngularVelocity = { 0.0f, 0.0f, 0.0f };
		double AngularDamping = 0.0;
		double Altitude = 0.0;

		RigidBodyComponent() = default;
		RigidBodyComponent(Vector3& centerOfMass, double invMass)
			: InvMass(invMass), CenterOfMass(centerOfMass) {}
	};

	struct SphereColliderComponent
	{
		Ref<ShapeSphere> Collider;

		Ref<Mesh> ColliderMesh;
		bool RenderCollider = false;

		SphereColliderComponent() = default;
		SphereColliderComponent(Ref<ShapeSphere>& collider, const Ref<Mesh>& mesh)
			: Collider(collider), ColliderMesh(mesh) {}
	};

	struct BoxColliderComponent
	{
		Ref<ShapeBox> Collider;

		Ref<Mesh> ColliderMesh;
		bool RenderCollider = false;

		BoxColliderComponent() = default;
		BoxColliderComponent(Ref<ShapeBox>& collider)
			: Collider(collider) {}
	};

	struct UIPanelComponent
	{
		Ref<UIPanel> Panel;

		UIPanelComponent() = default;
		UIPanelComponent(const UIPanelComponent&) = default;
		UIPanelComponent(const Ref<UIPanel>& panel)
			: Panel(panel) {}
	};

	struct UITextComponent 
	{
		Ref<UIText> Text;

		UITextComponent() = default;
		UITextComponent(const UITextComponent&) = default;
		UITextComponent(const Ref<UIText>& text)
			: Text(text) {}
	};

	struct UIButtonComponent 
	{
		Ref<UIButton> Button;

		UIButtonComponent() = default;
		UIButtonComponent(const UIButtonComponent&) = default;
		UIButtonComponent(const Ref<UIButton>& button)
			: Button(button) {}
	};

	struct TerrainObjectComponent
	{
		int SubdivisionActivation = 0;
		int MaxNrOfObjectPerFace = 0;
		int MaxNrOfObjects = 0;
		Ref<Mesh> MeshObject;

		TerrainObjectComponent() = default;
		TerrainObjectComponent(const TerrainObjectComponent& other) = default;
		TerrainObjectComponent(const Ref<Mesh>& mesh)
			: MeshObject(mesh) {}
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

		Texture2D* MaskTexture;

		ParticlesComponent() = default;
		ParticlesComponent(const ParticlesComponent& other) = default;
	};

}