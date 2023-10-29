#pragma once

#include <DirectXMath.h>

#include "Toast/Core/UUID.h"
#include "Toast/Core/Math/Math.h"

#include "Toast/Scene/SceneCamera.h"

#include "Toast/Renderer/Mesh.h"
#include "Toast/Renderer/PlanetSystem.h"
#include "Toast/Renderer/SceneEnvironment.h"

#include "Toast/Renderer/UI/UIElement.h"

#include "Toast/Physics/Bounds.h"
#include "Toast/Physics/Shapes.h"

#include <../vendor/directxtex/include/DirectXTex.h>

namespace Toast {

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

	struct PlanetComponent
	{
		struct PlanetGPUData
		{
			float radius = 3389.5f;
			float minAltitude = -8.2f;
			float maxAltitude = 21.2f;
			float gravAcc = 9.82f;
			float atmosphereHeight = 100.8f;
			float mieAnisotropy = 0.0f;
			float rayScaleHeight = 0.0f;
			float mieScaleHeight = 0.0f;
			DirectX::XMFLOAT3 rayBaseScatteringCoefficient = { 0.0f, 0.0f, 0.0f };
			float mieBaseScatteringCoefficient = 0.0f;
			DirectX::XMFLOAT3 planetCenter = { 0.0f, 0.0f, 0.0f };
			bool atmosphereToggle = false;
			int inScatteringPoints = 1;
			int opticalDepthPoints = 1;
		};

		Ref<Mesh> Mesh;
		std::vector<float> DistanceLUT;
		std::vector<float> FaceLevelDotLUT;
		std::vector<float> HeightMultLUT;
		int16_t Subdivisions = 0;
		
		std::unordered_map<Vertex, uint32_t, PlanetSystem::VertexHasher, PlanetSystem::VertexEquality> PlanetVertexMap;

		PlanetGPUData PlanetData;

		PlanetComponent() = default;
		PlanetComponent(int16_t subdivisions, int16_t patchLevels, float maxAltitude, float minAltitude, float radius, float gravAcc, float atmosphereHeight, bool atmosphereToggle, int inScatteringPoints, int opticalDepthPoints, float mieAnisotropy, float rayScaleHeight, float mieScaleHeight, DirectX::XMFLOAT3 rayBaseScatteringCoefficient, float mieBaseScatteringCoefficient)
			: Subdivisions(subdivisions)
		{
			PlanetData.maxAltitude = maxAltitude;
			PlanetData.minAltitude = minAltitude;
			PlanetData.radius = radius;
			PlanetData.gravAcc = gravAcc;
			PlanetData.atmosphereToggle = atmosphereToggle;

			PlanetData.atmosphereHeight = atmosphereHeight;
			PlanetData.inScatteringPoints = inScatteringPoints;
			PlanetData.opticalDepthPoints = opticalDepthPoints;
			PlanetData.mieAnisotropy = mieAnisotropy;
			PlanetData.rayScaleHeight = rayScaleHeight;
			PlanetData.mieScaleHeight = mieScaleHeight;
			PlanetData.rayBaseScatteringCoefficient = rayBaseScatteringCoefficient;
			PlanetData.mieBaseScatteringCoefficient = mieBaseScatteringCoefficient;

		}
		PlanetComponent(const PlanetComponent& other) = default;
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
		bool SunDisc = false;
	};

	struct SkyLightComponent
	{
		Environment SceneEnvironment;
		float Intensity = 1.0f;
	};

	struct RigidBodyComponent
	{
		double InvMass = 0.0f;
		double Elasticity = 0.0f;
		double Friction = 0.0f;
		Vector3 CenterOfMass = { 0.0f, 0.0f, 0.0f };
		Vector3 LinearVelocity = { 0.0f, 0.0f, 0.0f };
		Vector3 AngularVelocity = { 0.0f, 0.0f, 0.0f };

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

	struct TerrainColliderComponent 
	{
		Ref<ShapeTerrain> Collider;

		TerrainColliderComponent() = default;
		TerrainColliderComponent(const Ref<ShapeTerrain>& collider)
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

}