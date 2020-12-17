#pragma once

#include <DirectXMath.h>

#include "SceneCamera.h"
#include "ScriptableEntity.h"
#include "Toast/Renderer/Mesh.h"
#include "Toast/Renderer/PlanetSystem.h"

namespace Toast {

	struct TagComponent
	{
		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(const std::string& tag)
			: Tag(tag) {}
	};

	struct TransformComponent
	{
		DirectX::XMFLOAT3 Translation = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 Rotation = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 Scale = { 1.0f, 1.0f, 1.0f };

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const DirectX::XMFLOAT3& translation)
			: Translation(translation) {}

		DirectX::XMMATRIX GetTransform() const
		{
			DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity() * DirectX::XMMatrixScaling(Scale.x, Scale.y, Scale.z)
				* DirectX::XMMatrixRotationRollPitchYaw(Rotation.x, Rotation.y, Rotation.z)
				* DirectX::XMMatrixTranslation(Translation.x, Translation.y, Translation.z);

			return transform;
		}
	};

	struct MeshComponent
	{
		Ref<Toast::Mesh> Mesh;

		MeshComponent() = default;
		MeshComponent(const MeshComponent& other) = default;
		MeshComponent(const Ref<Toast::Mesh>& mesh)
			: Mesh(mesh) {}

		operator Ref<Toast::Mesh>() { return Mesh; }
	};

	struct PlanetComponent
	{
		struct MorphGPUData 
		{
			DirectX::XMFLOAT4 DistanceLUT[24];
			DirectX::XMFLOAT4 morphRange = { 0.5f, 0.5f, 0.5f, 0.5f };
		};

		struct PlanetGPUData
		{
			DirectX::XMFLOAT4 radius = { 3389.5f, 3389.5f, 3389.5f, 3389.5f };
			DirectX::XMFLOAT4 minAltitude = { -8.2f, -8.2f, -8.2f, -8.2f };
			DirectX::XMFLOAT4 maxAltitude = { 21.2f, 21.2f, 21.2f, 21.2f };
		};

		std::vector<float> FaceLevelDotLUT;
		int16_t Subdivisions = 0;
		int16_t PatchLevels = 1;

		MorphGPUData MorphData;
		PlanetGPUData PlanetData;

		PlanetComponent() = default;
		PlanetComponent(int16_t subdivisions, int16_t patchLevels, DirectX::XMFLOAT4 maxAltitude, DirectX::XMFLOAT4 minAltitude, DirectX::XMFLOAT4 radius)
			: Subdivisions(subdivisions), PatchLevels(patchLevels)
		{
			PlanetData.maxAltitude = maxAltitude;
			PlanetData.minAltitude = minAltitude;
			PlanetData.radius = radius;
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

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
	};

	struct NativeScriptComponent
	{
		ScriptableEntity* Instance = nullptr;

		ScriptableEntity* (*InstantiateScript)();
		void (*DestroyScript)(NativeScriptComponent*);

		template<typename T>
		void Bind()
		{
			InstantiateScript = []() { return static_cast<ScriptableEntity*>(new T()); };
			DestroyScript = [](NativeScriptComponent* nsc) { delete nsc->Instance; nsc->Instance = nullptr; };
		}
	};
}